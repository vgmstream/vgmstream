#include "layout.h"
#include "../vgmstream.h"

/* The Bouncer STRx blocks, one block per channel when stereo */
void block_update_vs_str(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_32bitLE(block_offset+0x04,streamFile); /* can be smaller than 0x800 */
    vgmstream->next_block_offset = block_offset + 0x800*vgmstream->channels;
    /* 0x08: number of remaning blocks, 0x10: some id/size? (shared in all blocks) */

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x20 + 0x800*i;
    }
}
