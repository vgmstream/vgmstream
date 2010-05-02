#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ps2_adm_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	vgmstream->current_block_offset = block_offset;
	vgmstream->current_block_size = 0x1000; /*read_32bitLE(
			vgmstream->current_block_offset+0x10,
			vgmstream->ch[0].streamfile); */
	vgmstream->next_block_offset = vgmstream->current_block_offset + vgmstream->current_block_size;
	//vgmstream->current_block_size/=vgmstream->channels;

	for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset+(0x400*i);
    }
}
