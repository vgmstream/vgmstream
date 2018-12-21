#include "layout.h"
#include "../vgmstream.h"

/* mini-blocks of size + data */
void block_update_vs(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->current_block_size = read_32bitLE(vgmstream->current_block_offset,streamFile);
        vgmstream->next_block_offset = vgmstream->current_block_offset + vgmstream->current_block_size + 0x04;
        vgmstream->ch[i].offset = vgmstream->current_block_offset + 0x04;
        if (i == 0) block_offset=vgmstream->next_block_offset;
    }
}
