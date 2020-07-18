#include "coding.h"

#ifdef VGM_USE_ATRAC9
#include "libatrac9.h"


/* opaque struct */
struct atrac9_codec_data {
    uint8_t* data_buffer;
    size_t data_buffer_size;

    sample_t* sample_buffer;
    size_t samples_filled; /* number of samples in the buffer */
    size_t samples_used; /* number of samples extracted from the buffer */

    int samples_to_discard;

    atrac9_config config;

    void* handle; /* decoder handle */
    Atrac9CodecInfo info; /* decoder info */
};


atrac9_codec_data* init_atrac9(atrac9_config* cfg) {
    int status;
    uint8_t config_data[4];
    atrac9_codec_data* data = NULL;

    data = calloc(1, sizeof(atrac9_codec_data));
    if (!data) goto fail;

    data->handle = Atrac9GetHandle();
    if (!data->handle) goto fail;

    put_32bitBE(config_data, cfg->config_data);
    status = Atrac9InitDecoder(data->handle, config_data);
    if (status < 0) goto fail;

    status = Atrac9GetCodecInfo(data->handle, &data->info);
    if (status < 0) goto fail;
    //;VGM_LOG("ATRAC9: config=%x, sf-size=%x, sub-frames=%i x %i samples\n", cfg->config_data, data->info.superframeSize, data->info.framesInSuperframe, data->info.frameSamples);

    if (cfg->channels && cfg->channels != data->info.channels) {
        VGM_LOG("ATRAC9: channels in header %i vs config %i don't match\n", cfg->channels, data->info.channels);
        goto fail; /* unknown multichannel layout */
    }


    /* must hold at least one superframe and its samples */
    data->data_buffer_size = data->info.superframeSize;
    /* extra leeway as Atrac9Decode seems to overread ~2 bytes (doesn't affect decoding though) */
    data->data_buffer = calloc(sizeof(uint8_t), data->data_buffer_size + 0x10);
    /* while ATRAC9 uses float internally, Sony's API only return PCM16 */
    data->sample_buffer = calloc(sizeof(sample_t), data->info.channels * data->info.frameSamples * data->info.framesInSuperframe);

    data->samples_to_discard = cfg->encoder_delay;

    memcpy(&data->config, cfg, sizeof(atrac9_config));

    return data;

fail:
    free_atrac9(data);
    return NULL;
}

void decode_atrac9(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels) {
    VGMSTREAMCHANNEL* stream = &vgmstream->ch[0];
    atrac9_codec_data* data = vgmstream->codec_data;
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

            data->samples_used = 0;

            /* ATRAC9 is made of decodable superframes with several sub-frames. AT9 config data gives
             * superframe size, number of frames and samples (~100-200 bytes and ~256/1024 samples). */

            /* read one raw block (superframe) and advance offsets */
            bytes = read_streamfile(data->data_buffer,stream->offset, data->info.superframeSize,stream->streamfile);
            if (bytes != data->data_buffer_size) goto decode_fail;

            stream->offset += bytes;

            /* decode all frames in the superframe block */
            for (iframe = 0; iframe < data->info.framesInSuperframe; iframe++) {
                status = Atrac9Decode(data->handle, buffer, data->sample_buffer + data->samples_filled*channels, &bytes_used);
                if (status < 0) goto decode_fail;

                buffer += bytes_used;
                data->samples_filled += data->info.frameSamples;
            }
        }
    }

    return;

decode_fail:
    /* on error just put some 0 samples */
    VGM_LOG("ATRAC9: decode fail at %x, missing %i samples\n", (uint32_t)stream->offset, (samples_to_do - samples_done));
    memset(outbuf + samples_done * channels, 0, (samples_to_do - samples_done) * sizeof(sample) * channels);
}

void reset_atrac9(atrac9_codec_data* data) {
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
    data->samples_to_discard = data->config.encoder_delay;

    return;

fail:
    return; /* decode calls should fail... */
}

void seek_atrac9(VGMSTREAM* vgmstream, int32_t num_sample) {
    atrac9_codec_data* data = vgmstream->codec_data;
    if (!data) return;

    reset_atrac9(data);

    /* find closest offset to desired sample, and samples to discard after that offset to reach loop */
    {
        int32_t seek_sample = data->config.encoder_delay + num_sample;
        off_t seek_offset;
        int32_t seek_discard;
        int32_t superframe_samples = data->info.frameSamples * data->info.framesInSuperframe;
        size_t superframe_number, superframe_back;

        superframe_number = (seek_sample / superframe_samples); /* closest */

        /* decoded frames affect each other slightly, so move offset back to make PCM stable
         * and equivalent to a full discard loop */
        superframe_back = 1; /* 1 seems enough (even when only 1 subframe in superframe) */
        if (superframe_back > superframe_number)
            superframe_back = superframe_number;

        seek_discard = (seek_sample % superframe_samples) + (superframe_back * superframe_samples);
        seek_offset  = (superframe_number - superframe_back) * data->info.superframeSize;

        data->samples_to_discard = seek_discard; /* already includes encoder delay */

        if (vgmstream->loop_ch)
            vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset + seek_offset;
    }

#if 0
    //old full discard loop
    {
        data->samples_to_discard = num_sample;
        data->samples_to_discard += data->config.encoder_delay;

        /* loop offsets are set during decode; force them to stream start so discard works */
        if (vgmstream->loop_ch)
            vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
    }
#endif

}

void free_atrac9(atrac9_codec_data* data) {
    if (!data) return;

    if (data->handle) Atrac9ReleaseHandle(data->handle);
    free(data->data_buffer);
    free(data->sample_buffer);
    free(data);
}


static int atrac9_parse_config(uint32_t atrac9_config, int *out_sample_rate, int *out_channels, size_t *out_frame_size, size_t *out_samples_per_frame) {
    static const int sample_rate_table[16] = {
            11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
            44100, 48000, 64000, 88200, 96000,128000,176400,192000
    };
    static const int samples_power_table[16] = {
        6, 6, 7, 7, 7, 8, 8, 8,
        6, 6, 7, 7, 7, 8, 8, 8
    };
    static const int channel_table[8] = {
            1, 2, 2, 6, 8, 4, 0, 0
    };

    int superframe_size, frames_per_superframe, samples_per_frame, samples_per_superframe;
    uint32_t sync             = (atrac9_config >> 24) & 0xff; /* 8b */
    uint8_t sample_rate_index = (atrac9_config >> 20) & 0x0f; /* 4b */
    uint8_t channels_index    = (atrac9_config >> 17) & 0x07; /* 3b */
    /* uint8_t validation bit = (atrac9_config >> 16) & 0x01; */ /* 1b */
    size_t frame_size         = (atrac9_config >>  5) & 0x7FF; /* 11b */
    size_t superframe_index   = (atrac9_config >>  3) & 0x3; /* 2b */
    /* uint8_t unused         = (atrac9_config >>  0) & 0x7);*/ /* 3b */

    superframe_size = ((frame_size+1) << superframe_index);
    frames_per_superframe = (1 << superframe_index);
    samples_per_frame = 1 << samples_power_table[sample_rate_index];
    samples_per_superframe = samples_per_frame * frames_per_superframe;

    if (sync != 0xFE)
        goto fail;
    if (out_sample_rate)
        *out_sample_rate = sample_rate_table[sample_rate_index];
    if (out_channels)
        *out_channels = channel_table[channels_index];
    if (out_frame_size)
        *out_frame_size = superframe_size;
    if (out_samples_per_frame)
        *out_samples_per_frame = samples_per_superframe;

    return 1;
fail:
    return 0;
}

size_t atrac9_bytes_to_samples(size_t bytes, atrac9_codec_data* data) {
    return bytes / data->info.superframeSize * (data->info.frameSamples * data->info.framesInSuperframe);
}

size_t atrac9_bytes_to_samples_cfg(size_t bytes, uint32_t atrac9_config) {
    size_t frame_size, samples_per_frame;
    if (!atrac9_parse_config(atrac9_config, NULL, NULL, &frame_size, &samples_per_frame))
        return 0;
    return bytes / frame_size * samples_per_frame;
}
#endif
