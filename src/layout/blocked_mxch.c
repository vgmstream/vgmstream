#include "layout.h"
#include "../vgmstream.h"

//MxCh blocked layout as used by Lego Island
void block_update_mxch(off_t block_offset, VGMSTREAM * vgmstream) {
	vgmstream->current_block_offset = block_offset;
	vgmstream->next_block_offset = block_offset +
		read_32bitLE(vgmstream->current_block_offset+4,vgmstream->ch[0].streamfile)+8;
    /* skip pad blocks */
    while (
        read_32bitBE(vgmstream->current_block_offset,
        vgmstream->ch[0].streamfile) == 0x70616420)
    {
        vgmstream->current_block_offset = vgmstream->next_block_offset;
        vgmstream->next_block_offset = vgmstream->current_block_offset +
            read_32bitLE(vgmstream->current_block_offset+4,vgmstream->ch[0].streamfile)+8;
    }
    vgmstream->current_block_size =
        read_32bitLE(vgmstream->current_block_offset+4, vgmstream->ch[0].streamfile)-0xe;
    // only one channel for now
    vgmstream->ch[0].offset = vgmstream->current_block_offset+8+0xe;
}
