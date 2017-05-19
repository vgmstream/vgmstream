#include "coding.h"

#ifdef VGM_USE_FFMPEG

/* internal sizes, can be any value */
#define FFMPEG_DEFAULT_BUFFER_SIZE 2048
#define FFMPEG_DEFAULT_IO_BUFFER_SIZE 128 * 1024


static volatile int g_ffmpeg_initialized = 0;


/* ******************************************** */
/* INTERNAL UTILS                               */
/* ******************************************** */

/* Global FFmpeg init */
static void g_init_ffmpeg() {
    if (g_ffmpeg_initialized == 1) {
        while (g_ffmpeg_initialized < 2); /* active wait for lack of a better way */
    }
    else if (g_ffmpeg_initialized == 0) {
        g_ffmpeg_initialized = 1;
        av_log_set_flags(AV_LOG_SKIP_REPEATED);
        av_log_set_level(AV_LOG_ERROR);
        av_register_all();
        g_ffmpeg_initialized = 2;
    }
}

/* converts codec's samples (can be in any format, ex. Ogg's float32) to PCM16 */
static void convert_audio(sample *outbuf, const uint8_t *inbuf, int sampleCount, int bitsPerSample, int floatingPoint) {
    int s;
    switch (bitsPerSample) {
        case 8:
        {
            for (s = 0; s < sampleCount; ++s) {
                *outbuf++ = ((int)(*(inbuf++))-0x80) << 8;
            }
        }
            break;
        case 16:
        {
            int16_t *s16 = (int16_t *)inbuf;
            for (s = 0; s < sampleCount; ++s) {
                *outbuf++ = *(s16++);
            }
        }
            break;
        case 32:
        {
            if (!floatingPoint) {
                int32_t *s32 = (int32_t *)inbuf;
                for (s = 0; s < sampleCount; ++s) {
                    *outbuf++ = (*(s32++)) >> 16;
                }
            }
            else {
                float *s32 = (float *)inbuf;
                for (s = 0; s < sampleCount; ++s) {
                    float sample = *s32++;
                    int s16 = (int)(sample * 32768.0f);
                    if ((unsigned)(s16 + 0x8000) & 0xFFFF0000) {
                        s16 = (s16 >> 31) ^ 0x7FFF;
                    }
                    *outbuf++ = s16;
                }
            }
        }
            break;
        case 64:
        {
            if (floatingPoint) {
                double *s64 = (double *)inbuf;
                for (s = 0; s < sampleCount; ++s) {
                    double sample = *s64++;
                    int s16 = (int)(sample * 32768.0f);
                    if ((unsigned)(s16 + 0x8000) & 0xFFFF0000) {
                        s16 = (s16 >> 31) ^ 0x7FFF;
                    }
                    *outbuf++ = s16;
                }
            }
        }
            break;
    }
}

/**
 * Special patching for FFmpeg's buggy seek code.
 *
 * To seek with avformat_seek_file/av_seek_frame, FFmpeg's demuxers can implement read_seek2 (newest API)
 * or read_seek (older API), with various search modes. If none are available it will use seek_frame_generic,
 * which manually reads frame by frame until the selected timestamp. However, the prev frame will be consumed
 * (so after seeking to 0 next av_read_frame will actually give the second frame and so on).
 *
 * Fortunately seek_frame_generic can use an index to find the correct position. This function reads the
 * first frame/packet and sets up index to timestamp 0. This ensures faulty demuxers will seek to 0 correctly.
 * Some formats may not seek to 0 even with this, though.
 */
static int init_seek(ffmpeg_codec_data * data) {
    int ret, ts_index, found_first = 0;
    int64_t ts = 0;
    int64_t pos = 0; /* offset */
    int size = 0; /* coded size */
    int distance = 0; /* always? */

    AVStream * stream;
    AVPacket * pkt;

    stream = data->formatCtx->streams[data->streamIndex];
    pkt = data->lastReadPacket;

    /* read_seek shouldn't need this index, but direct access to FFmpeg's internals is no good */
    /* if (data->formatCtx->iformat->read_seek || data->formatCtx->iformat->read_seek2)
        return 0; */

    /* some formats already have a proper index (e.g. M4A) */
    ts_index = av_index_search_timestamp(stream, ts, AVSEEK_FLAG_ANY);
    if (ts_index>=0)
        goto test_seek;


    /* find the first + second packets to get pos/size */
    while (1) {
        av_packet_unref(pkt);
        ret = av_read_frame(data->formatCtx, pkt);
        if (ret < 0)
            break;
        if (pkt->stream_index != data->streamIndex)
            continue; /* ignore non-selected streams */

        if (!found_first) { /* first found */
            found_first = 1;
            pos = pkt->pos;
            ts = pkt->dts;
            continue;
        } else { /* second found */
            size = pkt->pos - pos; /* coded, pkt->size is decoded size */
            break;
        }
    }
    if (!found_first)
        goto fail;

    /* in rare cases there is only one packet */
    /* if (size == 0) { size = data_end - pos; } */ /* no easy way to know, ignore (most formats don's need size) */

    /* some formats (XMA1) don't seem to have packet.dts, pretend it's 0 */
    if (ts == INT64_MIN)
        ts = 0;

    /* Some streams start with negative DTS (observed in Ogg). For Ogg seeking to negative or 0 doesn't alter the output.
     *  It does seem seeking before decoding alters a bunch of (inaudible) +-1 lower bytes though. */
    VGM_ASSERT(ts != 0, "FFMPEG: negative start_ts (%li)\n", (long)ts);
    if (ts != 0)
        ts = 0;

    /* add index 0 */
    ret = av_add_index_entry(stream, pos, ts, size, distance, AVINDEX_KEYFRAME);
    if ( ret < 0 )
        return ret;


test_seek:
    /* seek to 0 test / move back to beginning, since we just consumed packets */
    ret = avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    if ( ret < 0 )
        return ret; /* we can't even reset_vgmstream the file */

    avcodec_flush_buffers(data->codecCtx);

    return 0;

fail:
    return -1;
}


/* ******************************************** */
/* AVIO CALLBACKS                               */
/* ******************************************** */

/* AVIO callback: read stream, skipping external headers if needed */
static int ffmpeg_read(void *opaque, uint8_t *buf, int buf_size) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    uint64_t offset = data->offset;
    int max_to_copy = 0;
    int ret;

    if (data->header_insert_block) {
        if (offset < data->header_size) {
            max_to_copy = (int)(data->header_size - offset);
            if (max_to_copy > buf_size) {
                max_to_copy = buf_size;
            }
            memcpy(buf, data->header_insert_block + offset, max_to_copy);
            buf += max_to_copy;
            buf_size -= max_to_copy;
            offset += max_to_copy;
            if (!buf_size) {
                data->offset = offset;
                return max_to_copy;
            }
        }
        offset -= data->header_size;
    }

    /* when "fake" size is smaller than "real" size we need to make sure bytes_read (ret) is clamped;
     * it confuses FFmpeg in rare cases (STREAMFILE may have valid data after size) */
    if (offset + buf_size > data->size + data->header_size) {
        buf_size = data->size - offset; /* header "read" is manually inserted later */
    }

    ret = read_streamfile(buf, offset + data->start, buf_size, data->streamfile);
    if (ret > 0) {
        offset += ret;
        if (data->header_insert_block) {
            ret += max_to_copy;
        }
    }

    if (data->header_insert_block) {
        offset += data->header_size;
    }

    data->offset = offset;
    return ret;
}

/* AVIO callback: write stream not needed */
static int ffmpeg_write(void *opaque, uint8_t *buf, int buf_size) {
    return -1;
}

/* AVIO callback: seek stream, skipping external headers if needed */
static int64_t ffmpeg_seek(void *opaque, int64_t offset, int whence) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    int ret = 0;

    if (whence & AVSEEK_SIZE) {
        return data->size + data->header_size;
    }
    whence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);
    /* false offsets, on reads data->start will be added */
    switch (whence) {
        case SEEK_SET:
            break;

        case SEEK_CUR:
            offset += data->offset;
            break;

        case SEEK_END:
            offset += data->size;
            if (data->header_insert_block)
                offset += data->header_size;
            break;
    }

    /* clamp offset; fseek returns 0 when offset > size, too */
    if (offset > data->size + data->header_size) {
        offset = data->size + data->header_size;
    }

    data->offset = offset;
    return ret;
}


/* ******************************************** */
/* MAIN INIT/DECODER                            */
/* ******************************************** */

/**
 * Manually init FFmpeg, from an offset.
 * Used if the stream has internal data recognized by FFmpeg.
 */
ffmpeg_codec_data * init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset(streamFile, NULL, 0, start, size);
}

/**
 * Manually init FFmpeg, from a fake header / offset.
 *
 * Takes a fake header, to trick FFmpeg into demuxing/decoding the stream.
 * This header will be seamlessly inserted before 'start' offset, and total filesize will be 'header_size' + 'size'.
 * The header buffer will be copied and memory-managed internally.
 */
ffmpeg_codec_data * init_ffmpeg_header_offset(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size) {
    char filename[PATH_LIMIT];
    ffmpeg_codec_data * data;
    int errcode, i;

    int streamIndex, streamCount;
    AVStream *stream;
    AVCodecParameters *codecPar;

    AVRational tb;


    /* basic setup */
    g_init_ffmpeg();

    data = ( ffmpeg_codec_data * ) calloc(1, sizeof(ffmpeg_codec_data));
    if (!data) return NULL;

    streamFile->get_name( streamFile, filename, sizeof(filename) );

    data->streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!data->streamfile) goto fail;

    data->start = start;
    data->size = size;


    /* insert fake header to trick FFmpeg into demuxing/decoding the stream */
    if (header_size > 0) {
        data->header_size = header_size;
        data->header_insert_block = av_memdup(header, header_size);
        if (!data->header_insert_block) goto fail;
    }

    /* setup IO, attempt to autodetect format and gather some info */
    data->buffer = av_malloc(FFMPEG_DEFAULT_IO_BUFFER_SIZE);
    if (!data->buffer) goto fail;

    data->ioCtx = avio_alloc_context(data->buffer, FFMPEG_DEFAULT_IO_BUFFER_SIZE, 0, data, ffmpeg_read, ffmpeg_write, ffmpeg_seek);
    if (!data->ioCtx) goto fail;

    data->formatCtx = avformat_alloc_context();
    if (!data->formatCtx) goto fail;

    data->formatCtx->pb = data->ioCtx;

    if ((errcode = avformat_open_input(&data->formatCtx, "", NULL, NULL)) < 0) goto fail; /* autodetect */

    if ((errcode = avformat_find_stream_info(data->formatCtx, NULL)) < 0) goto fail;


    /* find valid audio stream inside */
    streamIndex = -1;
    streamCount = 0; /* audio streams only */

    for (i = 0; i < data->formatCtx->nb_streams; ++i) {
        stream = data->formatCtx->streams[i];
        codecPar = stream->codecpar;
        if (streamIndex < 0 && codecPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streamIndex = i; /* select first audio stream found */
        } else {
            stream->discard = AVDISCARD_ALL; /* disable demuxing unneded streams */
        }
        if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO)
            streamCount++;
    }

    if (streamIndex < 0) goto fail;

    data->streamIndex = streamIndex;
    stream = data->formatCtx->streams[streamIndex];
    data->streamCount = streamCount;


    /* prepare codec and frame/packet buffers */
    data->codecCtx = avcodec_alloc_context3(NULL);
    if (!data->codecCtx) goto fail;

    if ((errcode = avcodec_parameters_to_context(data->codecCtx, codecPar)) < 0) goto fail;

    av_codec_set_pkt_timebase(data->codecCtx, stream->time_base);

    data->codec = avcodec_find_decoder(data->codecCtx->codec_id);
    if (!data->codec) goto fail;

    if ((errcode = avcodec_open2(data->codecCtx, data->codec, NULL)) < 0) goto fail;

    data->lastDecodedFrame = av_frame_alloc();
    if (!data->lastDecodedFrame) goto fail;
    av_frame_unref(data->lastDecodedFrame);

    data->lastReadPacket = malloc(sizeof(AVPacket));
    if (!data->lastReadPacket) goto fail;
    av_new_packet(data->lastReadPacket, 0);

    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;


    /* other setup */
    data->sampleRate = data->codecCtx->sample_rate;
    data->channels = data->codecCtx->channels;
    data->floatingPoint = 0;

    switch (data->codecCtx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            data->bitsPerSample = 8;
            break;

        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            data->bitsPerSample = 16;
            break;

        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            data->bitsPerSample = 32;
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            data->bitsPerSample = 32;
            data->floatingPoint = 1;
            break;

        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            data->bitsPerSample = 64;
            data->floatingPoint = 1;
            break;

        default:
            goto fail;
    }

    data->bitrate = (int)(data->codecCtx->bit_rate);
    data->endOfStream = 0;
    data->endOfAudio = 0;

    /* try to guess frames/samples (duration isn't always set) */
    tb.num = 1; tb.den = data->codecCtx->sample_rate;
    data->totalSamples = av_rescale_q(stream->duration, stream->time_base, tb);
    if (data->totalSamples < 0)
        data->totalSamples = 0; /* caller must consider this */

    data->blockAlign = data->codecCtx->block_align;
    data->frameSize = data->codecCtx->frame_size;
    if(data->frameSize == 0) /* some formats don't set frame_size but can get on request, and vice versa */
        data->frameSize = av_get_audio_frame_duration(data->codecCtx,0);

    /* setup decode buffer */
    data->sampleBufferBlock = FFMPEG_DEFAULT_BUFFER_SIZE;
    data->sampleBuffer = av_malloc( data->sampleBufferBlock * (data->bitsPerSample / 8) * data->channels );
    if (!data->sampleBuffer)
        goto fail;


    /* setup decent seeking for faulty formats */
    errcode = init_seek(data);
    if (errcode < 0) goto fail;

    /* expose start samples to be skipped (encoder delay, usually added by MDCT-based encoders like AAC/MP3/ATRAC3/XMA/etc)
     * get after init_seek because some demuxers like AAC only fill skip_samples for the first packet */
    if (stream->start_skip_samples) /* samples to skip in the first packet */
        data->skipSamples = stream->start_skip_samples;
    else if (stream->skip_samples) /* samples to skip in any packet (first in this case), used sometimes instead (ex. AAC) */
        data->skipSamples = stream->skip_samples;

    return data;

fail:
    free_ffmpeg(data);

    return NULL;
}

/* decode samples of any kind of FFmpeg format */
void decode_ffmpeg(VGMSTREAM *vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
    
    int bytesPerSample, bytesPerFrame, frameSize;
    int bytesToRead, bytesRead;
    
    uint8_t *targetBuf;
    
    AVFormatContext *formatCtx;
    AVCodecContext *codecCtx;
    AVPacket *lastReadPacket;
    AVFrame *lastDecodedFrame;
    
    int bytesConsumedFromDecodedFrame;
    
    int readNextPacket, endOfStream, endOfAudio;
    int framesReadNow;
    

    /* ignore decode attempts at EOF */
    if (data->endOfStream || data->endOfAudio) {
        memset(outbuf, 0, samples_to_do * channels * sizeof(sample));
        return;
    }
    
    bytesPerSample = data->bitsPerSample / 8;
    bytesPerFrame = channels * bytesPerSample;
    frameSize = data->channels * bytesPerSample;
    
    bytesToRead = samples_to_do * frameSize;
    bytesRead = 0;
    
    targetBuf = data->sampleBuffer;
    memset(targetBuf, 0, bytesToRead);
    
    formatCtx = data->formatCtx;
    codecCtx = data->codecCtx;
    lastReadPacket = data->lastReadPacket;
    lastDecodedFrame = data->lastDecodedFrame;
    
    bytesConsumedFromDecodedFrame = data->bytesConsumedFromDecodedFrame;
    
    readNextPacket = data->readNextPacket;
    endOfStream = data->endOfStream;
    endOfAudio = data->endOfAudio;

    /* keep reading and decoding packets until the requested number of samples (in bytes) */
    while (bytesRead < bytesToRead) {
        int planeSize, planar, dataSize, toConsume, errcode;

        /* size of previous frame */
        dataSize = av_samples_get_buffer_size(&planeSize, codecCtx->channels, lastDecodedFrame->nb_samples, codecCtx->sample_fmt, 1);
        if (dataSize < 0)
            dataSize = 0;
        
        /* read new frame + packets when requested */
        while (readNextPacket && !endOfAudio) {
            if (!endOfStream) {
                av_packet_unref(lastReadPacket);
                if ((errcode = av_read_frame(formatCtx, lastReadPacket)) < 0) {
                    if (errcode == AVERROR_EOF) {
                        endOfStream = 1;
                    }
                    if (formatCtx->pb && formatCtx->pb->error)
                        break;
                }
                if (lastReadPacket->stream_index != data->streamIndex)
                    continue; /* ignore non-selected streams */
            }
            
            /* send compressed packet to decoder (NULL at EOF to "drain") */
            if ((errcode = avcodec_send_packet(codecCtx, endOfStream ? NULL : lastReadPacket)) < 0) {
                if (errcode != AVERROR(EAGAIN)) {
                    goto end;
                }
            }
            
            readNextPacket = 0;
        }
        
        /* decode packets into frame (checking if we have bytes to consume from previous frame) */
        if (dataSize <= bytesConsumedFromDecodedFrame) {
            if (endOfStream && endOfAudio)
                break;
            
            bytesConsumedFromDecodedFrame = 0;
            
            /* receive uncompressed data from decoder */
            if ((errcode = avcodec_receive_frame(codecCtx, lastDecodedFrame)) < 0) {
                if (errcode == AVERROR_EOF) {
                    endOfAudio = 1;
                    break;
                }
                else if (errcode == AVERROR(EAGAIN)) {
                    readNextPacket = 1;
                    continue;
                }
                else {
                    goto end;
                }
            }
            
            /* size of current frame */
            dataSize = av_samples_get_buffer_size(&planeSize, codecCtx->channels, lastDecodedFrame->nb_samples, codecCtx->sample_fmt, 1);
            if (dataSize < 0)
                dataSize = 0;
        }
        
        toConsume = FFMIN((dataSize - bytesConsumedFromDecodedFrame), (bytesToRead - bytesRead));
        
        /* discard decoded frame if needed (fully or partially) */
        if (data->samplesToDiscard) {
            int samplesDataSize = dataSize / bytesPerFrame;

            if (data->samplesToDiscard >= samplesDataSize) {
                /* discard all of the frame's samples and continue to the next */

                bytesConsumedFromDecodedFrame = dataSize;
                data->samplesToDiscard -= samplesDataSize;

                continue;
            }
            else {
                /* discard part of the frame and copy the rest below */
                int bytesToDiscard = data->samplesToDiscard * bytesPerFrame;
                int dataSizeLeft = dataSize - bytesToDiscard;

                bytesConsumedFromDecodedFrame += bytesToDiscard;
                data->samplesToDiscard = 0;
                if (toConsume > dataSizeLeft)
                    toConsume = dataSizeLeft; /* consume at most dataSize left */
            }
        }

        /* copy decoded frame to buffer (mux channels if needed) */
        planar = av_sample_fmt_is_planar(codecCtx->sample_fmt);
        if (!planar || channels == 1) {
            memmove(targetBuf + bytesRead, (lastDecodedFrame->data[0] + bytesConsumedFromDecodedFrame), toConsume);
        }
        else {
            uint8_t * out = (uint8_t *) targetBuf + bytesRead;
            int bytesConsumedPerPlane = bytesConsumedFromDecodedFrame / channels;
            int toConsumePerPlane = toConsume / channels;
            int s, ch;
            for (s = 0; s < toConsumePerPlane; s += bytesPerSample) {
                for (ch = 0; ch < channels; ++ch) {
                    memcpy(out, lastDecodedFrame->extended_data[ch] + bytesConsumedPerPlane + s, bytesPerSample);
                    out += bytesPerSample;
                }
            }
        }
        
        /* consume */
        bytesConsumedFromDecodedFrame += toConsume;
        bytesRead += toConsume;
    }
    
end:
    framesReadNow = bytesRead / frameSize;
    
    /* Convert the audio */
    convert_audio(outbuf, data->sampleBuffer, framesReadNow * channels, data->bitsPerSample, data->floatingPoint);
    
    /* Output the state back to the structure */
    data->bytesConsumedFromDecodedFrame = bytesConsumedFromDecodedFrame;
    data->readNextPacket = readNextPacket;
    data->endOfStream = endOfStream;
    data->endOfAudio = endOfAudio;
}


/* ******************************************** */
/* UTILS                                        */
/* ******************************************** */

void reset_ffmpeg(VGMSTREAM *vgmstream) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;

    if (data->formatCtx) {
        avformat_seek_file(data->formatCtx, data->streamIndex, 0, 0, 0, AVSEEK_FLAG_ANY);
    }
    if (data->codecCtx) {
        avcodec_flush_buffers(data->codecCtx);
    }
    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;
    data->endOfStream = 0;
    data->endOfAudio = 0;
    data->samplesToDiscard = 0;

    /* consider skip samples (encoder delay), if manually set (otherwise let FFmpeg handle it) */
    if (data->skipSamplesSet) {
        AVStream *stream = data->formatCtx->streams[data->streamIndex];
        /* sometimes (ex. AAC) after seeking to the first packet skip_samples is restored, but we want our value */
        stream->skip_samples = 0;
        stream->start_skip_samples = 0;

        data->samplesToDiscard += data->skipSamples;
    }
}

void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
    int64_t ts;

    /* Start from 0 and discard samples until loop_start (slower but not too noticeable).
     * Due to various FFmpeg quirks seeking to a sample is erratic in many formats (would need extra steps). */
    data->samplesToDiscard = num_sample;
    ts = 0;

    avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    avcodec_flush_buffers(data->codecCtx);

    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;
    data->endOfStream = 0;
    data->endOfAudio = 0;

    /* consider skip samples (encoder delay), if manually set (otherwise let FFmpeg handle it) */
    if (data->skipSamplesSet) {
        AVStream *stream = data->formatCtx->streams[data->streamIndex];
        /* sometimes (ex. AAC) after seeking to the first packet skip_samples is restored, but we want our value */
        stream->skip_samples = 0;
        stream->start_skip_samples = 0;

        data->samplesToDiscard += data->skipSamples;
    }
}

void free_ffmpeg(ffmpeg_codec_data *data) {
    if (data == NULL)
        return;

    if (data->lastReadPacket) {
        av_packet_unref(data->lastReadPacket);
        free(data->lastReadPacket);
        data->lastReadPacket = NULL;
    }
    if (data->lastDecodedFrame) {
        av_free(data->lastDecodedFrame);
        data->lastDecodedFrame = NULL;
    }
    if (data->codecCtx) {
        avcodec_close(data->codecCtx);
        avcodec_free_context(&(data->codecCtx));
        data->codecCtx = NULL;
    }
    if (data->formatCtx) {
        avformat_close_input(&(data->formatCtx));
        data->formatCtx = NULL;
    }
    if (data->ioCtx) {
        // buffer passed in is occasionally freed and replaced.
        // the replacement must be freed as well.
        data->buffer = data->ioCtx->buffer;
        av_free(data->ioCtx);
        data->ioCtx = NULL;
    }
    if (data->buffer) {
        av_free(data->buffer);
        data->buffer = NULL;
    }
    if (data->sampleBuffer) {
        av_free(data->sampleBuffer);
        data->sampleBuffer = NULL;
    }
    if (data->header_insert_block) {
        av_free(data->header_insert_block);
        data->header_insert_block = NULL;
    }
    if (data->streamfile) {
        close_streamfile(data->streamfile);
        data->streamfile = NULL;
    }
    free(data);
}


/**
 * Sets the number of samples to skip at the beginning of the stream, needed by some "gapless" formats.
 *  (encoder delay, usually added by MDCT-based encoders like AAC/MP3/ATRAC3/XMA/etc to "set up" the decoder).
 * - should be used at the beginning of the stream
 * - should check if there are data->skipSamples before using this, to avoid overwritting FFmpeg's value (ex. AAC).
 *
 * This could be added per format in FFmpeg directly, but it's here for flexibility and due to bugs
 *  (FFmpeg's stream->(start_)skip_samples causes glitches in XMA).
 */
void ffmpeg_set_skip_samples(ffmpeg_codec_data * data, int skip_samples) {
    AVStream *stream = NULL;
    if (!data->formatCtx)
        return;

    /* overwrite FFmpeg's skip samples */
    stream = data->formatCtx->streams[data->streamIndex];
    stream->start_skip_samples = 0; /* used for the first packet *if* pts=0 */
    stream->skip_samples = 0; /* skip_samples can be used for any packet */

    /* set skip samples with our internal discard */
    data->skipSamplesSet = 1;
    data->samplesToDiscard = skip_samples;

    /* expose (info only) */
    data->skipSamples = skip_samples;
}

#endif
