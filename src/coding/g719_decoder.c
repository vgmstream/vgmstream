#include "coding.h"

#ifdef VGM_USE_G719
#include <g719.h>

#define G719_MAX_CODES ((1280/8)) /* in int16, so max frame size is (value/8)*2 (0xF0=common, 0x140=decoder max 2560b, rare) */

struct g719_codec_data {
   sample_t buffer[960];
   void *handle;
};

g719_codec_data* init_g719(int channel_count, int frame_size) {
    int i;
    g719_codec_data* data = NULL;

    if (frame_size / sizeof(int16_t) > G719_MAX_CODES)
        goto fail;

    data = calloc(channel_count, sizeof(g719_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (i = 0; i < channel_count; i++) {
        data[i].handle = g719_init(frame_size); /* Siren 22 == 22khz bandwidth */
        if (!data[i].handle) goto fail;
    }

    return data;

fail:
    if (data) {
        for (i = 0; i < channel_count; i++) {
            g719_free(data[i].handle);
        }
    }
    free(data);

    return NULL;
}


void decode_g719(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL* ch = &vgmstream->ch[channel];
    g719_codec_data* data = vgmstream->codec_data;
    g719_codec_data* ch_data = &data[channel];
    int i;

    if (0 == vgmstream->samples_into_block) {
        int16_t code_buffer[G719_MAX_CODES];

        read_streamfile((uint8_t*)code_buffer, ch->offset, vgmstream->interleave_block_size, ch->streamfile);
        g719_decode_frame(ch_data->handle, code_buffer, ch_data->buffer);
    }

    for (i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}


void reset_g719(g719_codec_data* data, int channels) {
    int i;
    if (!data) return;

    for (i = 0; i < channels; i++) {
        g719_reset(data[i].handle);
    }
}

void free_g719(g719_codec_data* data, int channels) {
    int i;
    if (!data) return;

    for (i = 0; i < channels; i++) {
        g719_free(data[i].handle);
    }
    free(data);
}

#endif
