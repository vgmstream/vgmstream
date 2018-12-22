#include "layout.h"
#include "../vgmstream.h"

/* Square "VS" headered blocks */
void block_update_vs_square(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    size_t block_size = 0x800;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_size - 0x20;
    vgmstream->next_block_offset = block_offset + block_size*vgmstream->channels;
    /* 0x08: number of remaning blocks, 0x0c: blocks left */

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x20 + 0x800*i;
    }
}
