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
