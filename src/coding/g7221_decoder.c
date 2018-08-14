#include "coding.h"

#ifdef VGM_USE_G7221
#define G7221_MAX_CODES (960/8) /* max frame 0xF0 uint8s = 0xF0/2 uint16s = 960/8 */


g7221_codec_data * init_g7221(int channel_count, int frame_size) {
    int i;
    g7221_codec_data *data = NULL;

    if (frame_size / sizeof(int16_t) > G7221_MAX_CODES)
        goto fail;

    data = calloc(channel_count, sizeof(g7221_codec_data)); /* one decoder per channel */
    if (!data) goto fail;

    for (i = 0; i < channel_count; i++) {
        data[i].handle = g7221_init(frame_size, 14000); /* Siren 14 == 14khz bandwidth */
        if (!data[i].handle) goto fail;
    }

    return data;

fail:
    if (data) {
        for (i = 0; i < channel_count; i++) {
            g7221_free(data[i].handle);
        }
    }
    free(data);

    return NULL;
}


void decode_g7221(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL *ch = &vgmstream->ch[channel];
    g7221_codec_data *data = vgmstream->codec_data;
    g7221_codec_data *ch_data = &data[channel];
    int i;

    if (0 == vgmstream->samples_into_block) {
        int16_t code_buffer[G7221_MAX_CODES];
        vgmstream->ch[channel].streamfile->read(ch->streamfile, (uint8_t*)code_buffer, ch->offset, vgmstream->interleave_block_size);
        g7221_decode_frame(ch_data->handle, code_buffer, ch_data->buffer);
    }

    for (i = 0; i < samples_to_do; i++) {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}


void reset_g7221(VGMSTREAM *vgmstream) {
    g7221_codec_data *data = vgmstream->codec_data;
    int i;
    if (!data) return;

    for (i = 0; i < vgmstream->channels; i++) {
        g7221_reset(data[i].handle);
    }
}

void free_g7221(VGMSTREAM *vgmstream) {
    g7221_codec_data *data = (g7221_codec_data *) vgmstream->codec_data;
    int i;
    if (!data) return;

    for (i = 0; i < vgmstream->channels; i++) {
        g7221_free(data[i].handle);
    }
    free(data);
}

#endif
