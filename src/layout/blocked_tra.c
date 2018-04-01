#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset (first 32bytes is useless for decoding) */
void block_update_tra(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	vgmstream->current_block_offset = block_offset;
	vgmstream->current_block_size = 0x400;
	vgmstream->next_block_offset = vgmstream->current_block_offset+vgmstream->current_block_size+8;
	vgmstream->current_block_size/=vgmstream->channels;

	for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset+(vgmstream->current_block_size*i)+0x4*(i+1);
		
    }
}
