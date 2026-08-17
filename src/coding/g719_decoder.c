#include "coding.h"

#ifdef VGM_USE_G719
#include <g719.h>

#define G719_MAX_CODES (1280 / 8) // int16 codes
#define G719_MAX_BYTES (G719_MAX_CODES * sizeof(short)) // 0xF0=common, 0x140=decoder max 2560b, rare

struct g719_codec_data {
   sample_t buffer[960];
   void *handle;
};

g719_codec_data* init_g719(int channels, int frame_size) {
    g719_codec_data* data = NULL;

    if (frame_size > G719_MAX_BYTES)
        goto fail;

    data = calloc(channels, sizeof(g719_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (int i = 0; i < channels; i++) {
        data[i].handle = g719_init(frame_size); /* Siren 22 == 22khz bandwidth */
        if (!data[i].handle) goto fail;
    }

    return data;

fail:
    if (data) {
        for (int i = 0; i < channels; i++) {
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

    if (0 == vgmstream->samples_into_block) {
        int16_t code_buffer[G719_MAX_CODES];

        read_streamfile((uint8_t*)code_buffer, ch->offset, vgmstream->interleave_block_size, ch->streamfile);
        g719_decode_frame(ch_data->handle, code_buffer, ch_data->buffer);
    }

    for (int i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}


void reset_g719(g719_codec_data* data, int channels) {
    if (!data)
        return;

    for (int i = 0; i < channels; i++) {
        g719_reset(data[i].handle);
    }
}

void free_g719(g719_codec_data* data, int channels) {
    if (!data)
        return;

    for (int i = 0; i < channels; i++) {
        g719_free(data[i].handle);
    }
    free(data);
}

#endif
