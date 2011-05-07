//#include <stdio.h>
#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void mtaf_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_32bitLE(vgmstream->current_block_offset+4, vgmstream->ch[0].streamfile) * vgmstream->interleave_block_size;
    vgmstream->next_block_offset = block_offset + 8 + vgmstream->current_block_size * (vgmstream->channels/2);

    //printf("block at 0x%08lx has size %08lx\n", (unsigned long)block_offset, (unsigned long)vgmstream->current_block_size);

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset +
            8 + vgmstream->interleave_block_size * (i/2);
    }

}
