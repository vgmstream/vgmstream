#include "../vgmstream.h"

#ifdef VGM_USE_G7221
#include "g7221.h"
#include "coding.h"
#include "../util.h"

void decode_g7221(VGMSTREAM * vgmstream, 
        sample * outbuf, int channelspacing, int32_t samples_to_do, int channel) {
    VGMSTREAMCHANNEL *ch = &vgmstream->ch[channel];
    g7221_codec_data *data = vgmstream->codec_data;
    g7221_codec_data *ch_data = &data[channel];
    int i;

    if (0 == vgmstream->samples_into_block)
    {
        int16_t code_buffer[960/8];
        vgmstream->ch[channel].streamfile->read(ch->streamfile, (uint8_t*)code_buffer, ch->offset, vgmstream->interleave_block_size);
        g7221_decode_frame(ch_data->handle, code_buffer, ch_data->buffer);
    }

    for (i = 0; i < samples_to_do; i++)
    {
        outbuf[i*channelspacing] = ch_data->buffer[vgmstream->samples_into_block+i];
    }
}

#endif
