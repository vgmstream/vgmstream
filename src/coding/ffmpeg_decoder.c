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

void decode_ffmpeg(VGMSTREAM *vgmstream,
        sample * outbuf, int32_t samples_to_do, int channels) {
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
    if ((data->totalFrames && data->framesRead >= data->totalFrames) || data->endOfStream || data->endOfAudio) {
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
            int samplesToConsume;

            /* discard all if there are more samples to do than the packet's samples */
            if (data->samplesToDiscard >= dataSize / bytesPerFrame) {
                samplesToConsume = dataSize / bytesPerFrame;
            }
            else {
                samplesToConsume = toConsume / bytesPerFrame;
            }

            if (data->samplesToDiscard >= samplesToConsume) { /* full discard: skip to next */
                data->samplesToDiscard -= samplesToConsume;
                bytesConsumedFromDecodedFrame = dataSize;
                continue;
            }
            else { /* partial discard: copy below */
                bytesConsumedFromDecodedFrame += data->samplesToDiscard * bytesPerFrame;
                toConsume -= data->samplesToDiscard * bytesPerFrame;
                data->samplesToDiscard = 0;
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
    if (data->totalFrames && (data->framesRead + framesReadNow > data->totalFrames)) {
        framesReadNow = (int)(data->totalFrames - data->framesRead);
    }
    
    data->framesRead += framesReadNow;
    
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
    data->framesRead = 0;
    data->endOfStream = 0;
    data->endOfAudio = 0;
    data->samplesToDiscard = 0;
}


void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample) {
    ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
    int64_t ts;

#ifndef VGM_USE_FFMPEG_ACCURATE_LOOPING
    /* Seek to loop start by timestamp (closest frame) + adjust skipping some samples */
    /* FFmpeg seeks by ts by design (since not all containers can accurately skip to a frame). */
    /* TODO: this seems to be off by +-1 frames in some cases */
    ts = num_sample;
    if (ts >= data->sampleRate * 2) {
        data->samplesToDiscard = data->sampleRate * 2;
        ts -= data->samplesToDiscard;
    }
    else {
        data->samplesToDiscard = (int)ts;
        ts = 0;
    }

    /* todo fix this properly */
    if (data->totalFrames) {
        data->framesRead = (int)ts;
        ts = data->framesRead * (data->formatCtx->duration) / data->totalFrames;
    } else {
        data->samplesToDiscard = num_sample;
        data->framesRead = 0;
        ts = 0;
    }

    avformat_seek_file(data->formatCtx, data->streamIndex, ts - 1000, ts, ts, AVSEEK_FLAG_ANY);
    avcodec_flush_buffers(data->codecCtx);
#endif /* ifndef VGM_USE_FFMPEG_ACCURATE_LOOPING */

#ifdef VGM_USE_FFMPEG_ACCURATE_LOOPING
    /* Start from 0 and discard samples until loop_start for accurate looping (slower but not too noticeable) */
    /* We could also seek by offset (AVSEEK_FLAG_BYTE) to the frame closest to the loop then discard
     *  some samples, which is fast but would need calculations per format / when frame size is not constant */
    data->samplesToDiscard = num_sample;
    data->framesRead = 0;
    ts = 0;

    avformat_seek_file(data->formatCtx, data->streamIndex, ts, ts, ts, AVSEEK_FLAG_ANY);
    avcodec_flush_buffers(data->codecCtx);
#endif /* ifdef VGM_USE_FFMPEG_ACCURATE_LOOPING */

    data->readNextPacket = 1;
    data->bytesConsumedFromDecodedFrame = INT_MAX;
    data->endOfStream = 0;
    data->endOfAudio = 0;

}

#endif
