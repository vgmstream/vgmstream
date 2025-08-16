#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_halpst(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    // header length must be a multiple of 0x20 //TODO: needed?
    uint32_t header_size = (0x04 + 0x08 * vgmstream->channels + 0x1f) / 0x20 * 0x20;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_u32be(block_offset, sf) / vgmstream->channels;
    vgmstream->next_block_offset = read_u32be(block_offset + 0x08, sf);

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + vgmstream->current_block_size*i;
    }
}
