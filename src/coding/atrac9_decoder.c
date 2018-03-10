#include "coding.h"

#ifdef VGM_USE_ATRAC9
#include "libatrac9.h"


atrac9_codec_data *init_atrac9(atrac9_config *cfg) {
    int status;
    uint8_t config_data[4];
    Atrac9CodecInfo info = {0};
    atrac9_codec_data *data = NULL;

    data = calloc(1, sizeof(atrac9_codec_data));

    data->handle = Atrac9GetHandle();
    if (!data->handle) goto fail;

    put_32bitBE(config_data, cfg->config_data);
    status = Atrac9InitDecoder(data->handle, config_data);
    if (status < 0) goto fail;

    status = Atrac9GetCodecInfo(data->handle, &info);
    if (status < 0) goto fail;
    //;VGM_LOG("ATRAC9: config=%x, sf-size=%x, sub-frames=%i x %i samples\n", cfg->config_data, info.superframeSize, info.framesInSuperframe, info.frameSamples);

    if (cfg->channels && cfg->channels != info.channels) {
        VGM_LOG("ATRAC9: channels in header %i vs config %i don't match\n", cfg->channels, info.channels);
        goto fail; /* unknown multichannel layout */
    }


    /* must hold at least one superframe and its samples */
    data->data_buffer_size = info.superframeSize;
    data->data_buffer = calloc(sizeof(uint8_t), data->data_buffer_size);
    data->sample_buffer = calloc(sizeof(sample), info.channels * info.frameSamples * info.framesInSuperframe);

    data->samples_to_discard = cfg->encoder_delay;

    memcpy(&data->config, cfg, sizeof(atrac9_config));

    return data;

fail:
    return NULL;
}

void decode_atrac9(VGMSTREAM *vgmstream, sample * outbuf, int32_t samples_to_do, int channels) {
    VGMSTREAMCHANNEL *stream = &vgmstream->ch[0];
    atrac9_codec_data * data = vgmstream->codec_data;
    int samples_done = 0;


    while (samples_done < samples_to_do) {

        if (data->samples_filled) {  /* consume samples */
            int samples_to_get = data->samples_filled;

            if (data->samples_to_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_to_discard)
                    samples_to_get = data->samples_to_discard;
                data->samples_to_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;

                memcpy(outbuf + samples_done*channels,
                       data->sample_buffer + data->samples_used*channels,
                       samples_to_get*channels * sizeof(sample));

                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            data->samples_used += samples_to_get;
            data->samples_filled -= samples_to_get;
        }
        else { /* decode data */
            int iframe, status;
            int bytes_used = 0;
            uint8_t *buffer = data->data_buffer;
            size_t bytes;
            Atrac9CodecInfo info = {0};

            data->samples_used = 0;

            /* ATRAC9 is made of decodable superframes with several sub-frames. AT9 config data gives
             * superframe size, number of frames and samples (~100-200 bytes and ~256/1024 samples). */
            status = Atrac9GetCodecInfo(data->handle, &info);
            if (status < 0) goto decode_fail;


            /* preadjust */ //todo improve
            switch(data->config.type) {
                case ATRAC9_XVAG:
                    /* PS4 (ex. The Last of Us) has a RIFF AT9 (can be ignored) instead of the first superframe.
                     * As subsongs do too, needs to be skipped here instead of adjusting start_offset */
                    if (stream->offset == stream->channel_start_offset) {
                        if (read_32bitBE(stream->offset, stream->streamfile) == 0x00000000  /* padding before RIFF */
                                && read_32bitBE(stream->offset + info.superframeSize - 0x08,stream->streamfile) == 0x64617461) { /* RIFF's "data" */
                            stream->offset += info.superframeSize;
                        }
                    }
                    break;
                default:
                    break;
            }

            /* read one raw block (superframe) and advance offsets */
            bytes = read_streamfile(data->data_buffer,stream->offset, info.superframeSize,stream->streamfile);
            if (bytes != data->data_buffer_size) {
                VGM_LOG("ATRAC9: read %x vs expected %x bytes  at %lx\n", bytes, info.superframeSize, stream->offset);
                goto decode_fail;
            }

            stream->offset += bytes;

            /* postadjust */ //todo improve
            switch(data->config.type) {
                case ATRAC9_XVAG:
                case ATRAC9_KMA9:
                    /* skip other subsong blocks */
                    if (data->config.interleave_skip && ((stream->offset - stream->channel_start_offset) % data->config.interleave_skip == 0)) {
                        stream->offset += data->config.interleave_skip * (data->config.subsong_skip - 1);
                    }
                    break;
                default:
                    break;
            }


            /* decode all frames in the superframe block */
            for (iframe = 0; iframe < info.framesInSuperframe; iframe++) {
                status = Atrac9Decode(data->handle, buffer, data->sample_buffer + data->samples_filled*channels, &bytes_used);
                if (status < 0) goto decode_fail;

                buffer += bytes_used;
                data->samples_filled += info.frameSamples;
            }
        }
    }

    return;

decode_fail:
    /* on error just put some 0 samples */
    VGM_LOG("ATRAC9: decode fail at %lx, missing %i samples\n", stream->offset, (samples_to_do - samples_done));
    memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample) * channels);
}

void reset_atrac9(VGMSTREAM *vgmstream) {
    atrac9_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    if (!data->handle)
        goto fail;

#if 0
    /* reopen/flush, not needed as superframes decode separatedly and there is no carried state */
    {
        int status;
        uint8_t config_data[4];

        Atrac9ReleaseHandle(data->handle);
        data->handle = Atrac9GetHandle();
        if (!data->handle) goto fail;

        put_32bitBE(config_data, data->config.config_data);
        status = Atrac9InitDecoder(data->handle, config_data);
        if (status < 0) goto fail;
    }
#endif

    data->samples_used = 0;
    data->samples_filled = 0;
    data->samples_to_discard = 0;

    return;

fail:
    return; /* decode calls should fail... */
}

void seek_atrac9(VGMSTREAM *vgmstream, int32_t num_sample) {
    atrac9_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    reset_atrac9(vgmstream);

    data->samples_to_discard = num_sample;
    data->samples_to_discard += data->config.encoder_delay;

    /* loop offsets are set during decode; force them to stream start so discard works */
    if (vgmstream->loop_ch)
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
}

void free_atrac9(atrac9_codec_data *data) {
    if (!data) return;

    if (data->handle) Atrac9ReleaseHandle(data->handle);
    free(data->data_buffer);
    free(data->sample_buffer);
    free(data);
}


size_t atrac9_bytes_to_samples(size_t bytes, atrac9_codec_data *data) {
    Atrac9CodecInfo info = {0};
    int status;

    status = Atrac9GetCodecInfo(data->handle, &info);
    if (status < 0) goto fail;

    return bytes / info.superframeSize * (info.frameSamples * info.framesInSuperframe);

fail:
    return 0;
}
#endif
