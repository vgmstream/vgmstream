#include "layout.h"
#include "../vgmstream.h"

/* .wsi - headered blocks with a single channel */
void block_update_wsi(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    off_t channel_block_size;


    /* assume that all channels have the same size for this block */
    channel_block_size = read_32bitBE(block_offset, streamFile);

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = channel_block_size - 0x10; /* remove header */
    vgmstream->next_block_offset = block_offset + channel_block_size*vgmstream->channels;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + channel_block_size*i + 0x10;
    }

    /* first block has DSP header, remove */
    if (block_offset == vgmstream->ch[0].channel_start_offset) {
        vgmstream->current_block_size -= 0x60;
        for (i = 0; i < vgmstream->channels; i++) {
            vgmstream->ch[i].offset += 0x60;
        }
    }
}
