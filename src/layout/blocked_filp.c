#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_filp(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_u32le(block_offset + 0x18,sf) - 0x800;
    vgmstream->next_block_offset = block_offset + vgmstream->current_block_size + 0x800;
    vgmstream->current_block_size /= vgmstream->channels;

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x800+(vgmstream->current_block_size*i);
    }
}
