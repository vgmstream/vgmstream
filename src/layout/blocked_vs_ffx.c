#include "layout.h"
#include "../vgmstream.h"

/* Final Fantasy X VS headered blocks */
void block_update_vs_ffx(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    size_t block_size = 0x800;

    /* 0x00: header id
     * 0x04: null
     * 0x08: block number
     * 0x0c: blocks left in the subfile
     * 0x10: always 0x1000
     * 0x14: always 0x64
     * 0x18: null
     * 0x1c: null */

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_size - 0x20;
    vgmstream->next_block_offset = block_offset + block_size;
    /* 0x08: number of remaning blocks, 0x10: some id/size? (shared in all blocks) */

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x20 + 0x800*i;
    }
}
