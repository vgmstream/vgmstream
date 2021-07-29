#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"


/* XVAG with subsongs layers, interleaves chunks of each subsong (a hack to support them) */
void block_update_xvag_subsong(off_t block_offset, VGMSTREAM* vgmstream) {
    int i;
    size_t channel_size = 0x10;

    /* set offsets */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + channel_size*i;
    }

    //vgmstream->current_block_size = ; /* fixed */
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + vgmstream->full_block_size;
}
