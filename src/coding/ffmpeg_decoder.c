#include "coding.h"

#ifdef VGM_USE_FFMPEG

/* internal sizes, can be any value */
#define FFMPEG_DEFAULT_SAMPLE_BUFFER_SIZE 2048
#define FFMPEG_DEFAULT_IO_BUFFER_SIZE 128 * 1024


static volatile int g_ffmpeg_initialized = 0;

static void free_ffmpeg_config(ffmpeg_codec_data *data);
static int init_ffmpeg_config(ffmpeg_codec_data * data, int target_subsong, int reset);

static void reset_ffmpeg_internal(ffmpeg_codec_data *data);
static void seek_ffmpeg_internal(ffmpeg_codec_data *data, int32_t num_sample);

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
        //av_register_all(); /* not needed in newer versions */
        g_ffmpeg_initialized = 2;
    }
}

static void remap_audio(sample_t *outbuf, int sample_count, int channels, int channel_mappings[]) {
    int ch_from,ch_to,s;
    sample_t temp;
    for (s = 0; s < sample_count; s++) {
        for (ch_from = 0; ch_from < channels; ch_from++) {
            if (ch_from > 32)
                continue;

            ch_to = channel_mappings[ch_from];
            if (ch_to < 1 || ch_to > 32 || ch_to > channels-1 || ch_from == ch_to)
                continue;

            temp = outbuf[s*channels + ch_from];
            outbuf[s*channels + ch_from] = outbuf[s*channels + ch_to];
            outbuf[s*channels + ch_to] = temp;
        }
    }
}

static void invert_audio(sample_t *outbuf, int sample_count, int channels) {
    int i;

    for (i = 0; i < sample_count*channels; i++) {
        outbuf[i] = -outbuf[i];
    }
}

/* converts codec's samples (can be in any format, ex. Ogg's float32) to PCM16 */
static void convert_audio_pcm16(sample_t *outbuf, const uint8_t *inbuf, int fullSampleCount, int bitsPerSample, int floatingPoint) {
    int s;
    switch (bitsPerSample) {
        case 8: {
            for (s = 0; s < fullSampleCount; s++) {
                *outbuf++ = ((int)(*(inbuf++))-0x80) << 8;
            }
            break;
        }
        case 16: {
            int16_t *s16 = (int16_t *)inbuf;
            for (s = 0; s < fullSampleCount; s++) {
                *outbuf++ = *(s16++);
            }
            break;
        }
        case 32: {
            if (!floatingPoint) {
                int32_t *s32 = (int32_t *)inbuf;
                for (s = 0; s < fullSampleCount; s++) {
                    *outbuf++ = (*(s32++)) >> 16;
                }
            }
            else {
                float *s32 = (float *)inbuf;
                for (s = 0; s < fullSampleCount; s++) {
                    float sample = *s32++;
                    int s16 = (int)(sample * 32768.0f);
                    if ((unsigned)(s16 + 0x8000) & 0xFFFF0000) {
                        s16 = (s16 >> 31) ^ 0x7FFF;
                    }
                    *outbuf++ = s16;
                }
            }
            break;
        }
        case 64: {
            if (floatingPoint) {
                double *s64 = (double *)inbuf;
                for (s = 0; s < fullSampleCount; s++) {
                    double sample = *s64++;
                    int s16 = (int)(sample * 32768.0f);
                    if ((unsigned)(s16 + 0x8000) & 0xFFFF0000) {
                        s16 = (s16 >> 31) ^ 0x7FFF;
                    }
                    *outbuf++ = s16;
                }
            }
            break;
        }
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
    int ret, ts_index, packet_count = 0;
    int64_t ts = 0; /* seek timestamp */
    int64_t pos = 0; /* data offset */
    int size = 0; /* data size (block align) */
    int distance = 0; /* always 0 ("duration") */

    AVStream * stream = data->formatCtx->streams[data->streamIndex];
    AVPacket * pkt = data->lastReadPacket;


    /* read_seek shouldn't need this index, but direct access to FFmpeg's internals is no good */
    /* if (data->formatCtx->iformat->read_seek || data->formatCtx->iformat->read_seek2)
        return 0; */

    /* A few formats may have a proper index (e.g. CAF/MP4/MPC/ASF/WAV/XWMA/FLAC/MP3), but some don't
     * work with our custom index (CAF/MPC/MP4) and must skip it. Most formats need flag AVSEEK_FLAG_ANY,
     * while XWMA (with index 0 not pointing to ts 0) needs AVSEEK_FLAG_BACKWARD to seek properly, but it
     * makes OGG use the index and seek wrong instead. So for XWMA we forcefully remove the index on it's own meta. */
    ts_index = av_index_search_timestamp(stream, 0, /*AVSEEK_FLAG_BACKWARD |*/ AVSEEK_FLAG_ANY);
    if (ts_index >= 0) {
        VGM_LOG("FFMPEG: index found for init_seek\n");
        goto test_seek;
    }


    /* find the first + second packets to get pos/size */
    packet_count = 0;
    while (1) {
        av_packet_unref(pkt);
        ret = av_read_frame(data->formatCtx, pkt);
        if (ret < 0)
            break;
        if (pkt->stream_index != data->streamIndex)
            continue; /* ignore non-selected streams */

        //;VGM_LOG("FFMPEG: packet %i, ret=%i, pos=%i, dts=%i\n", packet_count, ret, (int32_t)pkt->pos, (int32_t)pkt->dts);
        packet_count++;
        if (packet_count == 1) {
            pos = pkt->pos;
            ts = pkt->dts;
            continue;
        } else { /* second found */
            size = pkt->pos - pos; /* coded, pkt->size is decoded size */
            break;
        }
    }
    if (packet_count == 0)
        goto fail;

    /* happens in unseekable formats where FFmpeg doesn't even know its own position */
    if (pos < 0)
        goto fail;

    /* in rare cases there is only one packet */
    //if (size == 0) size = data_end - pos; /* no easy way to know, ignore (most formats don's need size) */

    /* some formats don't seem to have packet.dts, pretend it's 0 */
    if (ts == INT64_MIN)
        ts = 0;

    /* Some streams start with negative DTS (OGG/OPUS). For Ogg seeking to negative or 0 doesn't seem different.
     * It does seem seeking before decoding alters a bunch of (inaudible) +-1 lower bytes though.
     * Output looks correct (encoder delay, num_samples, etc) compared to libvorbis's output. */
    VGM_ASSERT(ts != 0, "FFMPEG: negative start_ts (%li)\n", (long)ts);
    if (ts != 0)
        ts = 0;


    /* add index 0 */
    ret = av_add_index_entry(stream, pos, ts, size, distance, AVINDEX_KEYFRAME);
    if ( ret < 0 )
        return ret;

test_seek:
    /* seek to 0 test + move back to beginning, since we just consumed packets */
    ret = avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    if ( ret < 0 ) {
        //char test[1000] = {0}; av_strerror(ret, test, 1000); VGM_LOG("FFMPEG: ret=%i %s\n", ret, test);
        return ret; /* we can't even reset_vgmstream the file */
    }

    avcodec_flush_buffers(data->codecCtx);

    return 0;

fail:
    return -1;
}


/* ******************************************** */
/* AVIO CALLBACKS                               */
/* ******************************************** */

/* AVIO callback: read stream, handling custom data */
static int ffmpeg_read(void *opaque, uint8_t *buf, int read_size) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    int bytes = 0;
    int max_to_copy = 0;

    /* clamp reads */
    if (data->logical_offset + read_size > data->logical_size)
        read_size = data->logical_size - data->logical_offset;
    if (read_size == 0)
        return bytes;

    /* handle reads on inserted header */
    if (data->header_size && data->logical_offset < data->header_size) {
        max_to_copy = (int)(data->header_size - data->logical_offset);
        if (max_to_copy > read_size)
            max_to_copy = read_size;

        memcpy(buf, data->header_insert_block + data->logical_offset, max_to_copy);
        buf += max_to_copy;
        read_size -= max_to_copy;
        data->logical_offset += max_to_copy;

        if (read_size == 0) {
            return max_to_copy; /* offset still in header */
        }
    }

    /* main read */
    bytes = read_streamfile(buf, data->offset, read_size, data->streamfile);
    data->logical_offset += bytes;
    data->offset += bytes;
    return bytes + max_to_copy;
}

/* AVIO callback: write stream not needed */
static int ffmpeg_write(void *opaque, uint8_t *buf, int buf_size) {
    return -1;
}

/* AVIO callback: seek stream, handling custom data */
static int64_t ffmpeg_seek(void *opaque, int64_t offset, int whence) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) opaque;
    int ret = 0;

    /* get cache'd size */
    if (whence & AVSEEK_SIZE) {
        return data->logical_size;
    }

    whence &= ~(AVSEEK_SIZE | AVSEEK_FORCE);
    /* find the final offset FFmpeg sees (within fake header + virtual size) */
    switch (whence) {
        case SEEK_SET: /* absolute */
            break;

        case SEEK_CUR: /* relative to current */
            offset += data->logical_offset;
            break;

        case SEEK_END: /* relative to file end (should be negative) */
            offset += data->logical_size;
            break;
    }

    /* clamp offset; fseek does this too */
    if (offset > data->logical_size)
        offset = data->logical_size;
    else if (offset < 0)
        offset = 0;

    /* seeks inside fake header */
    if (offset < data->header_size) {
        data->logical_offset = offset;
        data->offset = data->start;
        return ret;
    }

    /* main seek */
    data->logical_offset = offset;
    data->offset = data->start + (offset - data->header_size);
    return ret;
}

/* ******************************************** */
/* MAIN INIT/DECODER                            */
/* ******************************************** */

ffmpeg_codec_data * init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset(streamFile, NULL,0, start,size);
}

ffmpeg_codec_data * init_ffmpeg_header_offset(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size) {
    return init_ffmpeg_header_offset_subsong(streamFile, header, header_size, start, size, 0);
}


/**
 * Manually init FFmpeg, from a fake header / offset.
 *
 * Takes a fake header, to trick FFmpeg into demuxing/decoding the stream.
 * This header will be seamlessly inserted before 'start' offset, and total filesize will be 'header_size' + 'size'.
 * The header buffer will be copied and memory-managed internally.
 * NULL header can used given if the stream has internal data recognized by FFmpeg at offset.
 * Stream index can be passed if the file has multiple audio streams that FFmpeg can demux (1=first).
 */
ffmpeg_codec_data * init_ffmpeg_header_offset_subsong(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size, int target_subsong) {
    char filename[PATH_LIMIT];
    ffmpeg_codec_data * data = NULL;
    int errcode;

    AVStream *stream;
    AVRational tb;


    /* check values */
    if ((header && !header_size) || (!header && header_size))
        goto fail;

    if (size == 0 || start + size > get_streamfile_size(streamFile)) {
        VGM_LOG("FFMPEG: wrong start+size found: %x + %x > %x \n", (uint32_t)start, (uint32_t)size, get_streamfile_size(streamFile));
        size = get_streamfile_size(streamFile) - start;
    }


    /* ffmpeg global setup */
    g_init_ffmpeg();


    /* basic setup */
    data = calloc(1, sizeof(ffmpeg_codec_data));
    if (!data) return NULL;

    streamFile->get_name( streamFile, filename, sizeof(filename) );
    data->streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!data->streamfile) goto fail;

    /* fake header to trick FFmpeg into demuxing/decoding the stream */
    if (header_size > 0) {
        data->header_size = header_size;
        data->header_insert_block = av_memdup(header, header_size);
        if (!data->header_insert_block) goto fail;
    }

    data->start = start;
    data->offset = data->start;
    data->size = size;
    data->logical_offset = 0;
    data->logical_size = data->header_size + data->size;


    /* setup FFmpeg's internals, attempt to autodetect format and gather some info */
    errcode = init_ffmpeg_config(data, target_subsong, 0);
    if (errcode < 0) goto fail;

    stream = data->formatCtx->streams[data->streamIndex];


    /* derive info */
    data->sampleRate = data->codecCtx->sample_rate;
    data->channels = data->codecCtx->channels;
    data->bitrate = (int)(data->codecCtx->bit_rate);
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

    /* setup decode buffer */
    data->sampleBufferBlock = FFMPEG_DEFAULT_SAMPLE_BUFFER_SIZE;
    data->sampleBuffer = av_malloc(data->sampleBufferBlock * (data->bitsPerSample / 8) * data->channels);
    if (!data->sampleBuffer) goto fail;


    /* try to guess frames/samples (duration isn't always set) */
    tb.num = 1; tb.den = data->codecCtx->sample_rate;
    data->totalSamples = av_rescale_q(stream->duration, stream->time_base, tb);
    if (data->totalSamples < 0)
        data->totalSamples = 0; /* caller must consider this */

    data->blockAlign = data->codecCtx->block_align;
    data->frameSize = data->codecCtx->frame_size;
    if(data->frameSize == 0) /* some formats don't set frame_size but can get on request, and vice versa */
        data->frameSize = av_get_audio_frame_duration(data->codecCtx,0);


    /* reset */
    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;
    data->endOfStream = 0;
    data->endOfAudio = 0;


    /* expose start samples to be skipped (encoder delay, usually added by MDCT-based encoders like AAC/MP3/ATRAC3/XMA/etc)
     * get after init_seek because some demuxers like AAC only fill skip_samples for the first packet */
    if (stream->start_skip_samples) /* samples to skip in the first packet */
        data->skipSamples = stream->start_skip_samples;
    else if (stream->skip_samples) /* samples to skip in any packet (first in this case), used sometimes instead (ex. AAC) */
        data->skipSamples = stream->skip_samples;


    /* check ways to skip encoder delay/padding, for debugging purposes (some may be old/unused/encoder only/etc) */
    VGM_ASSERT(data->codecCtx->delay > 0, "FFMPEG: delay %i\n", (int)data->codecCtx->delay);//delay: OPUS
    //VGM_ASSERT(data->codecCtx->internal->skip_samples > 0, ...); /* for codec use, not accessible */
    VGM_ASSERT(stream->codecpar->initial_padding > 0, "FFMPEG: initial_padding %i\n", (int)stream->codecpar->initial_padding);//delay: OPUS
    VGM_ASSERT(stream->codecpar->trailing_padding > 0, "FFMPEG: trailing_padding %i\n", (int)stream->codecpar->trailing_padding);
    VGM_ASSERT(stream->codecpar->seek_preroll > 0, "FFMPEG: seek_preroll %i\n", (int)stream->codecpar->seek_preroll);//seek delay: OPUS
    VGM_ASSERT(stream->skip_samples > 0, "FFMPEG: skip_samples %i\n", (int)stream->skip_samples); //delay: MP4
    VGM_ASSERT(stream->start_skip_samples > 0, "FFMPEG: start_skip_samples %i\n", (int)stream->start_skip_samples); //delay: MP3
    VGM_ASSERT(stream->first_discard_sample > 0, "FFMPEG: first_discard_sample %i\n", (int)stream->first_discard_sample); //padding: MP3
    VGM_ASSERT(stream->last_discard_sample > 0, "FFMPEG: last_discard_sample %i\n", (int)stream->last_discard_sample); //padding: MP3
    /* also negative timestamp for formats like OGG/OPUS */
    /* not using it: BINK, FLAC, ATRAC3, XMA, MPC, WMA (may use internal skip samples) */
    //todo: double check Opus behavior


    /* setup decent seeking for faulty formats */
    errcode = init_seek(data);
    if (errcode < 0) {
        VGM_LOG("FFMPEG: can't init_seek, error=%i\n", errcode);
        /* some formats like Smacker are so buggy that any seeking is impossible (even on video players)
         * whatever, we'll just kill and reconstruct FFmpeg's config every time */
        data->force_seek = 1;
        reset_ffmpeg_internal(data); /* reset state from trying to seek */
        //stream = data->formatCtx->streams[data->streamIndex];
    }

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

static int init_ffmpeg_config(ffmpeg_codec_data * data, int target_subsong, int reset) {
    int errcode = 0;

    /* basic IO/format setup */
    data->buffer = av_malloc(FFMPEG_DEFAULT_IO_BUFFER_SIZE);
    if (!data->buffer) goto fail;

    data->ioCtx = avio_alloc_context(data->buffer, FFMPEG_DEFAULT_IO_BUFFER_SIZE, 0, data, ffmpeg_read, ffmpeg_write, ffmpeg_seek);
    if (!data->ioCtx) goto fail;

    data->formatCtx = avformat_alloc_context();
    if (!data->formatCtx) goto fail;

    data->formatCtx->pb = data->ioCtx;

    //on reset could use AVFormatContext.iformat to reload old format
    errcode = avformat_open_input(&data->formatCtx, "", NULL, NULL);
    if (errcode < 0) goto fail;

    errcode = avformat_find_stream_info(data->formatCtx, NULL);
    if (errcode < 0) goto fail;

    /* find valid audio stream and set other streams to discard */
    {
        int i, streamIndex, streamCount;

        streamIndex = -1;
        streamCount = 0;
        if (reset)
            streamIndex = data->streamIndex;

        for (i = 0; i < data->formatCtx->nb_streams; ++i) {
            AVStream *stream = data->formatCtx->streams[i];

            if (stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                streamCount++;

                /* select Nth audio stream if specified, or first one */
                if (streamIndex < 0 || (target_subsong > 0 && streamCount == target_subsong)) {
                    streamIndex = i;
                }
            }

            if (i != streamIndex)
                stream->discard = AVDISCARD_ALL; /* disable demuxing for other streams */
        }
        if (streamCount < target_subsong) goto fail;
        if (streamIndex < 0) goto fail;

        data->streamIndex = streamIndex;
        data->streamCount = streamCount;
    }

    /* setup codec with stream info */
    data->codecCtx = avcodec_alloc_context3(NULL);
    if (!data->codecCtx) goto fail;

    errcode = avcodec_parameters_to_context(data->codecCtx, ((AVStream*)data->formatCtx->streams[data->streamIndex])->codecpar);
    if (errcode < 0) goto fail;

    //av_codec_set_pkt_timebase(data->codecCtx, stream->time_base); /* deprecated and seemingly not needed */

    data->codec = avcodec_find_decoder(data->codecCtx->codec_id);
    if (!data->codec) goto fail;

    errcode = avcodec_open2(data->codecCtx, data->codec, NULL);
    if (errcode < 0) goto fail;

    /* prepare codec and frame/packet buffers */
    data->lastDecodedFrame = av_frame_alloc();
    if (!data->lastDecodedFrame) goto fail;
    av_frame_unref(data->lastDecodedFrame);

    data->lastReadPacket = malloc(sizeof(AVPacket));
    if (!data->lastReadPacket) goto fail;
    av_new_packet(data->lastReadPacket, 0);

    return 0;
fail:
    if (errcode < 0)
        return errcode;
    return -1;
}


/* decode samples of any kind of FFmpeg format */
void decode_ffmpeg(VGMSTREAM *vgmstream, sample_t * outbuf, int32_t samples_to_do, int channels) {
    ffmpeg_codec_data *data = vgmstream->codec_data;
    int samplesReadNow;
    //todo use either channels / data->channels / codecCtx->channels

    AVFormatContext *formatCtx = data->formatCtx;
    AVCodecContext *codecCtx = data->codecCtx;
    AVPacket *packet = data->lastReadPacket;
    AVFrame *frame = data->lastDecodedFrame;

    int readNextPacket = data->readNextPacket;
    int endOfStream = data->endOfStream;
    int endOfAudio = data->endOfAudio;
    int bytesConsumedFromDecodedFrame = data->bytesConsumedFromDecodedFrame;

    int planar = 0;
    int bytesPerSample = data->bitsPerSample / 8;
    int bytesRead, bytesToRead;


    if (data->bad_init) {
        memset(outbuf, 0, samples_to_do * channels * sizeof(sample));
        return;
    }

    /* ignore once file is done (but not at endOfStream as FFmpeg can still output samples until endOfAudio) */
    if (/*endOfStream ||*/ endOfAudio) {
        VGM_LOG("FFMPEG: decode after end of audio\n");
        memset(outbuf, 0, samples_to_do * channels * sizeof(sample));
        return;
    }

    planar = av_sample_fmt_is_planar(codecCtx->sample_fmt);
    bytesRead = 0;
    bytesToRead = samples_to_do * (bytesPerSample * codecCtx->channels);


    /* keep reading and decoding packets until the requested number of samples (in bytes for FFmpeg calcs) */
    while (bytesRead < bytesToRead) {
        int dataSize, toConsume, errcode;

        /* get sample data size from current frame (dataSize will be < 0 when nb_samples = 0) */
        dataSize = av_samples_get_buffer_size(NULL, codecCtx->channels, frame->nb_samples, codecCtx->sample_fmt, 1);
        if (dataSize < 0)
            dataSize = 0;

        /* read new data packet when requested */
        while (readNextPacket && !endOfAudio) {
            if (!endOfStream) {
                /* reset old packet */
                av_packet_unref(packet);

                /* get compressed data from demuxer into packet */
                errcode = av_read_frame(formatCtx, packet);
                if (errcode < 0) {
                    if (errcode == AVERROR_EOF) {
                        endOfStream = 1; /* no more data, but may still output samples */
                    }
                    else {
                        VGM_LOG("FFMPEG: av_read_frame errcode %i\n", errcode);
                    }

                    if (formatCtx->pb && formatCtx->pb->error) {
                        break;
                    }
                }

                if (packet->stream_index != data->streamIndex)
                    continue; /* ignore non-selected streams */
            }

            /* send compressed data to decoder in packet (NULL at EOF to "drain") */
            errcode = avcodec_send_packet(codecCtx, endOfStream ? NULL : packet);
            if (errcode < 0) {
                if (errcode != AVERROR(EAGAIN)) {
                    VGM_LOG("FFMPEG: avcodec_send_packet errcode %i\n", errcode);
                    goto end;
                }
            }

            readNextPacket = 0; /* got compressed data */
        }

        /* decode packet into frame's sample data (if we don't have bytes to consume from previous frame) */
        if (dataSize <= bytesConsumedFromDecodedFrame) {
            if (endOfAudio) {
                break;
            }

            bytesConsumedFromDecodedFrame = 0;

            /* receive uncompressed sample data from decoder in frame */
            errcode = avcodec_receive_frame(codecCtx, frame);
            if (errcode < 0) {
                if (errcode == AVERROR_EOF) {
                    endOfAudio = 1; /* no more samples, file is fully decoded */
                    break;
                }
                else if (errcode == AVERROR(EAGAIN)) {
                    readNextPacket = 1; /* request more compressed data */
                    continue;
                }
                else {
                    VGM_LOG("FFMPEG: avcodec_receive_frame errcode %i\n", errcode);
                    goto end;
                }
            }

            /* get sample data size of current frame */
            dataSize = av_samples_get_buffer_size(NULL, codecCtx->channels, frame->nb_samples, codecCtx->sample_fmt, 1);
            if (dataSize < 0)
                dataSize = 0;
        }

        toConsume = FFMIN((dataSize - bytesConsumedFromDecodedFrame), (bytesToRead - bytesRead));


        /* discard decoded frame if needed (fully or partially) */
        if (data->samplesToDiscard) {
            int samplesDataSize = dataSize / (bytesPerSample * channels);

            if (data->samplesToDiscard >= samplesDataSize) {
                /* discard all of the frame's samples and continue to the next */
                bytesConsumedFromDecodedFrame = dataSize;
                data->samplesToDiscard -= samplesDataSize;
                continue;
            }
            else {
                /* discard part of the frame and copy the rest below */
                int bytesToDiscard = data->samplesToDiscard * (bytesPerSample * channels);
                int dataSizeLeft = dataSize - bytesToDiscard;

                bytesConsumedFromDecodedFrame += bytesToDiscard;
                data->samplesToDiscard = 0;
                if (toConsume > dataSizeLeft)
                    toConsume = dataSizeLeft;
            }
        }


        /* copy decoded sample data to buffer */
        if (!planar || channels == 1) { /* 1 sample per channel, already mixed */
            memmove(data->sampleBuffer + bytesRead, (frame->data[0] + bytesConsumedFromDecodedFrame), toConsume);
        }
        else { /* N samples per channel, mix to 1 sample per channel */
            uint8_t * out = (uint8_t *) data->sampleBuffer + bytesRead;
            int bytesConsumedPerPlane = bytesConsumedFromDecodedFrame / channels;
            int toConsumePerPlane = toConsume / channels;
            int s, ch;
            for (s = 0; s < toConsumePerPlane; s += bytesPerSample) {
                for (ch = 0; ch < channels; ++ch) {
                    memcpy(out, frame->extended_data[ch] + bytesConsumedPerPlane + s, bytesPerSample);
                    out += bytesPerSample;
                }
            }
        }

        /* consume */
        bytesConsumedFromDecodedFrame += toConsume;
        bytesRead += toConsume;
    }


end:
    /* convert native sample format into PCM16 outbuf */
    samplesReadNow = bytesRead / (bytesPerSample * channels);
    convert_audio_pcm16(outbuf, data->sampleBuffer, samplesReadNow * channels, data->bitsPerSample, data->floatingPoint);
    if (data->channel_remap_set)
        remap_audio(outbuf, samplesReadNow, data->channels, data->channel_remap);
    if (data->invert_audio_set)
        invert_audio(outbuf, samplesReadNow, data->channels);

    /* clean buffer when requested more samples than possible */
    if (endOfAudio && samplesReadNow < samples_to_do) {
        VGM_LOG("FFMPEG: decode after end of audio %i samples\n", (samples_to_do - samplesReadNow));
        memset(outbuf + (samplesReadNow * channels), 0, (samples_to_do - samplesReadNow) * channels * sizeof(sample));
    }

    /* copy state back */
    data->readNextPacket = readNextPacket;
    data->endOfStream = endOfStream;
    data->endOfAudio = endOfAudio;
    data->bytesConsumedFromDecodedFrame = bytesConsumedFromDecodedFrame;
}


/* ******************************************** */
/* UTILS                                        */
/* ******************************************** */

void reset_ffmpeg_internal(ffmpeg_codec_data *data) {
    seek_ffmpeg_internal(data, 0);
}
void reset_ffmpeg(VGMSTREAM *vgmstream) {
    reset_ffmpeg_internal(vgmstream->codec_data);
}

void seek_ffmpeg_internal(ffmpeg_codec_data *data, int32_t num_sample) {
    if (!data) return;

    /* Start from 0 and discard samples until sample (slower but not too noticeable).
     * Due to various FFmpeg quirks seeking to a sample is erratic in many formats (would need extra steps). */

    if (data->force_seek) {
        int errcode;

        /* kill+redo everything to allow seeking for extra-buggy formats,
         * kinda horrid but seems fast enough and very few formats need this */

        free_ffmpeg_config(data);

        data->offset = data->start;
        data->logical_offset = 0;

        errcode = init_ffmpeg_config(data, 0, 1);
        if (errcode < 0) goto fail;
    }
    else {
        avformat_seek_file(data->formatCtx, data->streamIndex, 0, 0, 0, AVSEEK_FLAG_ANY);
        avcodec_flush_buffers(data->codecCtx);
    }

    data->samplesToDiscard = num_sample;

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

    return;
fail:
    VGM_LOG("FFMPEG: error during force_seek\n");
    data->bad_init = 1; /* internals were probably free'd */
}

void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    seek_ffmpeg_internal(vgmstream->codec_data, num_sample);
}


static void free_ffmpeg_config(ffmpeg_codec_data *data) {
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
}

void free_ffmpeg(ffmpeg_codec_data *data) {
    if (data == NULL)
        return;

    free_ffmpeg_config(data);

    if (data->sampleBuffer) {
        av_free(data->sampleBuffer);
        data->sampleBuffer = NULL;
    }
    if (data->header_insert_block) {
        av_free(data->header_insert_block);
        data->header_insert_block = NULL;
    }

    close_streamfile(data->streamfile);
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
    if (!data || !data->formatCtx)
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

/* returns channel layout if set */
uint32_t ffmpeg_get_channel_layout(ffmpeg_codec_data * data) {
    if (!data || !data->codecCtx) return 0;
    return (uint32_t)data->codecCtx->channel_layout; /* uint64 but there ain't so many speaker mappings */
}

/* yet another hack to fix codecs that encode channels in different order and reorder on decoder
 * but FFmpeg doesn't do it automatically
 * (maybe should be done via mixing, but could clash with other stuff?) */
void ffmpeg_set_channel_remapping(ffmpeg_codec_data * data, int *channel_remap) {
    int i;

    if (data->channels > 32)
        return;

    for (i = 0; i < data->channels; i++) {
        data->channel_remap[i] = channel_remap[i];
    }
    data->channel_remap_set = 1;
}

#endif
