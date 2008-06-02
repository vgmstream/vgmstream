#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void xbox_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = 36*vgmstream->channels;
    vgmstream->next_block_offset = vgmstream->current_block_offset+(off_t)vgmstream->current_block_size;

    for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset +
            4*i;
    }
}
