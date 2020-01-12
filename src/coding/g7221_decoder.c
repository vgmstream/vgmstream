#include "coding.h"
#include "g7221_decoder_lib.h"

#ifdef VGM_USE_G7221
#define G7221_MAX_FRAME_SIZE 0x78 /* max frame 0x78 uint8s = 960/8 */

struct g7221_codec_data {
    sample_t buffer[640];
    g7221_handle *handle;
};

g7221_codec_data* init_g7221(int channel_count, int frame_size) {
    int i;
    g7221_codec_data* data = NULL;

    if (frame_size > G7221_MAX_FRAME_SIZE)
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
        uint8_t buf[G7221_MAX_FRAME_SIZE];
        size_t bytes;
        size_t read = vgmstream->interleave_block_size;

        bytes = vgmstream->ch[channel].streamfile->read(ch->streamfile, buf, ch->offset, read);
        if (bytes != read) {
            //g7221_decode_empty(ch_data->handle, ch_data->buffer);
            memset(ch_data->buffer, 0, sizeof(ch_data->buffer));
            VGM_LOG("S14: EOF read\n");
        }
        else {
            g7221_decode_frame(ch_data->handle, buf, ch_data->buffer);
        }
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
