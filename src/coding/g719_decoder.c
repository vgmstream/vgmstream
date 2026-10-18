#include "coding.h"

#ifdef VGM_USE_G719
#include "libs/g719_dec.h"

#define G719_MAX_CODES (1280 / 8) // int16 codes
#define G719_MAX_BYTES (G719_MAX_CODES * sizeof(short)) // 0xF0=common, 0x140=decoder max 2560b, rare

struct g719_codec_data {
   short pbuf[G719_MAX_FRAME_SAMPLES];
   void* handle;
   int frame_size;
};

g719_codec_data* init_g719(int channels, int frame_size) {
    g719_codec_data* data = NULL;

    if (frame_size > G719_MAX_FRAME_SIZE)
        return NULL;

    data = calloc(channels, sizeof(g719_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (int i = 0; i < channels; i++) {
        data[i].handle = g719_init(frame_size); /* Siren 22 == 22khz bandwidth */
        if (!data[i].handle) goto fail;

        data[i].frame_size = frame_size;
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
        uint8_t code_buffer[G719_MAX_FRAME_SIZE];

        read_streamfile(code_buffer, ch->offset, ch_data->frame_size, ch->streamfile);
        g719_decode_frame(ch_data->handle, code_buffer, ch_data->pbuf);
    }

    for (int i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->pbuf[vgmstream->samples_into_block+i];
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
