#include "../vgmstream.h"

#ifdef VGM_USE_FFMPEG

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

void decode_ffmpeg(VGMSTREAM *vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
    
    int bytesPerSample;
    int bytesPerFrame;
    int frameSize;
    
    int bytesToRead;
    int bytesRead;
    
    uint8_t *targetBuf;
    
    AVFormatContext *formatCtx;
    AVCodecContext *codecCtx;
    AVPacket *lastReadPacket;
    AVFrame *lastDecodedFrame;
    
    int bytesConsumedFromDecodedFrame;
    
    int readNextPacket;
    int endOfStream;
    int endOfAudio;
    
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
        int planeSize;
        int planar;
        int dataSize;
        int toConsume;
        int errcode;


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
    
    // Convert the audio
    convert_audio(outbuf, data->sampleBuffer, framesReadNow * channels, data->bitsPerSample, data->floatingPoint);
    
    // Output the state back to the structure
    data->bytesConsumedFromDecodedFrame = bytesConsumedFromDecodedFrame;
    data->readNextPacket = readNextPacket;
    data->endOfStream = endOfStream;
    data->endOfAudio = endOfAudio;
}


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
}


void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
    int64_t ts;

    /* Start from 0 and discard samples until loop_start (slower but not too noticeable) */
    /* Due to various FFmpeg quirks seeking to a sample is erratic in many formats (would need extra steps) */
    data->samplesToDiscard = num_sample;
    ts = 0;

    avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    avcodec_flush_buffers(data->codecCtx);

    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;
    data->endOfStream = 0;
    data->endOfAudio = 0;

}



/* ******************************************** */
/* FAKE RIFF HELPERS                            */
/* ******************************************** */
static int ffmpeg_fmt_chunk_swap_endian(uint8_t * chunk, uint16_t codec);

/**
 * Copies a ATRAC3 riff to buf
 *
 * returns number of bytes in buf or -1 when buf is not big enough
 */
int ffmpeg_make_riff_atrac3(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay) {
    uint16_t codec_ATRAC3 = 0x0270;
    size_t riff_size = 4+4+ 4 + 0x28 + 0x10 + 4+4;

    if (buf_size < riff_size)
        return -1;

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x20);/*fmt size*/
    put_16bitLE(buf+0x14, codec_ATRAC3);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_32bitLE(buf+0x20, (int16_t)(block_align)); /* block align */

    put_16bitLE(buf+0x24, 0x0e); /* extra data size */
    put_16bitLE(buf+0x26, 1); /* unknown, always 1 */
    put_16bitLE(buf+0x28, channels==1 ? 0x0800 : 0x1000); /* unknown (some size? 0x1000=2ch, 0x0800=1ch) */
    put_16bitLE(buf+0x2a, 0); /* unknown, always 0 */
    put_16bitLE(buf+0x2c, joint_stereo ? 0x0001 : 0x0000);
    put_16bitLE(buf+0x2e, joint_stereo ? 0x0001 : 0x0000); /* repeated? */
    put_16bitLE(buf+0x30, 1); /* unknown, always 1 (frame_factor?) */
    put_16bitLE(buf+0x32, 0); /* unknown, always 0 */

    memcpy(buf+0x34, "fact", 4);
    put_32bitLE(buf+0x38, 0x8); /* fact size */
    put_32bitLE(buf+0x3c, sample_count);
    put_32bitLE(buf+0x40, encoder_delay);

    memcpy(buf+0x44, "data", 4);
    put_32bitLE(buf+0x48, data_size); /* data size */

    return riff_size;
}

/**
 * Copies a XMA2 riff to buf
 *
 * returns number of bytes in buf or -1 when buf is not big enough
 */
int ffmpeg_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size) {
    uint16_t codec_XMA2 = 0x0166;
    size_t riff_size = 4+4+ 4 + 0x3c + 4+4;
    size_t bytecount;
    uint32_t streams = 0;
    uint32_t speakers = 0; /* see audiodefs.h */

    if (buf_size < riff_size)
        return -1;

    bytecount = sample_count * channels * sizeof(sample);

    /* untested (no support for > 2ch xma for now) */
    switch (channels) {
        case 1:
            streams = 1;
            speakers = 0x00000004; /* FC */
            break;
        case 2:
            streams = 1;
            speakers = 0x00000001 | 0x00000002; /* FL FR */
            break;
        case 3:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000004; /* FL FC FR */
            break;
        case 4:
            streams = 2;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020; /* FL FR BL BR */
            break;
        case 5:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020 | 0x00000004; /* FL C FR BL BR*/
            break;
        case 6:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020 | 0x00000200 | 0x00000400; /* FL FR BL BR SL SR */
            break;
         default:
            streams = 1;
            speakers = 0x80000000;
            break;
    }

    /*memset(buf,0, sizeof(uint8_t) * fmt_size);*/

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x34);/*fmt size*/
    put_16bitLE(buf+0x14, codec_XMA2);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_16bitLE(buf+0x20, (int16_t)(channels*sizeof(sample))); /* block align */
    put_16bitLE(buf+0x22, sizeof(sample)*8); /* bits per sample */

    put_16bitLE(buf+0x24, 0x22); /* extra data size */
    put_16bitLE(buf+0x26, streams); /* number of streams */
    put_32bitLE(buf+0x28, speakers); /* speaker position  */
    put_32bitLE(buf+0x2c, bytecount); /* PCM samples */
    put_32bitLE(buf+0x30, block_size); /* XMA block size */
    /* (looping values not set, expected to be handled externally) */
    put_32bitLE(buf+0x34, 0); /* play begin */
    put_32bitLE(buf+0x38, 0); /* play length */
    put_32bitLE(buf+0x3c, 0); /* loop begin */
    put_32bitLE(buf+0x40, 0); /* loop length */
    put_8bit(buf+0x44, 0); /* loop count */
    put_8bit(buf+0x45, 4); /* encoder version */
    put_16bitLE(buf+0x46, block_count); /* blocks count = entried in seek table */

    memcpy(buf+0x48, "data", 4);
    put_32bitLE(buf+0x4c, data_size); /* data size */

    return riff_size;
}


/**
 * Copies a XMA2 riff to buf from a fmt chunk offset
 *
 * returns number of bytes in buf or -1 when buf is not big enough
 */
int ffmpeg_make_riff_xma2_from_fmt(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian) {
    size_t riff_size = 4+4+ 4 + 4+4+fmt_size + 4+4;
    uint8_t chunk[100];

    if (buf_size < riff_size || fmt_size > 100)
        goto fail;
    if (read_streamfile(chunk,fmt_offset,fmt_size, streamFile) != fmt_size)
        goto fail;

    if (big_endian)
        ffmpeg_fmt_chunk_swap_endian(chunk, 0x166);

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, fmt_size);/*fmt size*/
    memcpy(buf+0x14, chunk, fmt_size);

    memcpy(buf+0x14+fmt_size, "data", 4);
    put_32bitLE(buf+0x14+fmt_size+4, data_size); /* data size */

    return riff_size;

fail:
    return -1;
}

/**
 * Swaps endianness
 *
 * returns 0 on error
 */
static int ffmpeg_fmt_chunk_swap_endian(uint8_t * chunk, uint16_t codec) {
    if (codec != 0x166)
        goto fail;

    put_16bitLE(chunk + 0x00, get_16bitBE(chunk + 0x00));/*wFormatTag*/
    put_16bitLE(chunk + 0x02, get_16bitBE(chunk + 0x02));/*nChannels*/
    put_32bitLE(chunk + 0x04, get_32bitBE(chunk + 0x04));/*nSamplesPerSec*/
    put_32bitLE(chunk + 0x08, get_32bitBE(chunk + 0x08));/*nAvgBytesPerSec*/
    put_16bitLE(chunk + 0x0c, get_16bitBE(chunk + 0x0c));/*nBlockAlign*/
    put_16bitLE(chunk + 0x0e, get_16bitBE(chunk + 0x0e));/*wBitsPerSample*/
    put_16bitLE(chunk + 0x10, get_16bitBE(chunk + 0x10));/*cbSize*/
    put_16bitLE(chunk + 0x12, get_16bitBE(chunk + 0x12));/*NumStreams*/
    put_32bitLE(chunk + 0x14, get_32bitBE(chunk + 0x14));/*ChannelMask*/
    put_32bitLE(chunk + 0x18, get_32bitBE(chunk + 0x18));/*SamplesEncoded*/
    put_32bitLE(chunk + 0x1c, get_32bitBE(chunk + 0x1c));/*BytesPerBlock*/
    put_32bitLE(chunk + 0x20, get_32bitBE(chunk + 0x20));/*PlayBegin*/
    put_32bitLE(chunk + 0x24, get_32bitBE(chunk + 0x24));/*PlayLength*/
    put_32bitLE(chunk + 0x28, get_32bitBE(chunk + 0x28));/*LoopBegin*/
    put_32bitLE(chunk + 0x2c, get_32bitBE(chunk + 0x2c));/*LoopLength*/
    /* put_8bit(chunk + 0x30,    get_8bit(chunk + 0x30));*//*LoopCount*/
    /* put_8bit(chunk + 0x31,    get_8bit(chunk + 0x31));*//*EncoderVersion*/
    put_16bitLE(chunk + 0x32, get_16bitBE(chunk + 0x32));/*BlockCount*/

    return 1;

fail:
    return 0;
}

#endif
