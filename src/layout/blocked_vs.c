#include "layout.h"
#include "../vgmstream.h"

/* mini-blocks of size + data */
void block_update_vs_mh(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->current_block_size = read_32bitLE(vgmstream->current_block_offset,sf);
        vgmstream->next_block_offset = vgmstream->current_block_offset + vgmstream->current_block_size + 0x04;
        vgmstream->ch[i].offset = vgmstream->current_block_offset + 0x04;
        if (i == 0) block_offset=vgmstream->next_block_offset;
    }
}
