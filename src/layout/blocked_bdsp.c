#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_bdsp(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	vgmstream->current_block_offset = block_offset;
	vgmstream->current_block_size = read_32bitBE(vgmstream->current_block_offset,vgmstream->ch[0].streamfile)/7*8;
	vgmstream->next_block_offset = vgmstream->current_block_offset + vgmstream->current_block_size+0xC0;

        for (i=0;i<vgmstream->channels;i++) {

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=vgmstream->current_block_offset*i;

        }
}
