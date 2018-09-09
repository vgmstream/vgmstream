#include "coding.h"
#include <g719.h>

#ifdef VGM_USE_G719
#define G719_MAX_FRAME_SIZE 0x1000 /* arbitrary max (samples per frame seems to always be 960) */


g719_codec_data *init_g719(int channel_count, int frame_size) {
    int i;
    g719_codec_data *data = NULL;

    if (frame_size / sizeof(int16_t) > G719_MAX_FRAME_SIZE)
        goto fail;

    data = calloc(channel_count, sizeof(g719_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (i = 0; i < channel_count; i++) {
        data[i].handle = g719_init(frame_size); /* Siren 22 == 22khz bandwidth */
        if (!data[i].handle) goto fail;

        /* known values: 0xF0=common (sizeof(int16) * 960/8), 0x140=rare (sizeof(int16) * 1280/8) */
        data[i].code_buffer = malloc(sizeof(int16_t) * frame_size);
        if (!data[i].code_buffer) goto fail;
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


void decode_g719(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL *ch = &vgmstream->ch[channel];
    g719_codec_data *data = vgmstream->codec_data;
    g719_codec_data *ch_data = &data[channel];
    int i;

    if (0 == vgmstream->samples_into_block) {
        read_streamfile((uint8_t*)ch_data->code_buffer, ch->offset, vgmstream->interleave_block_size, ch->streamfile);
        g719_decode_frame(ch_data->handle, ch_data->code_buffer, ch_data->buffer);
    }

    for (i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}


void reset_g719(g719_codec_data * data, int channels) {
    int i;
    if (!data) return;

    for (i = 0; i < channels; i++) {
        g719_reset(data[i].handle);
    }
}

void free_g719(g719_codec_data * data, int channels) {
    int i;
    if (!data) return;

    for (i = 0; i < channels; i++) {
        g719_free(data[i].handle);
        free(data[i].code_buffer);
    }
    free(data);
}

#endif
