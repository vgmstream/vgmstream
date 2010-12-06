#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ps2_iab_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	vgmstream->current_block_offset = block_offset;
	vgmstream->current_block_size = 0x4010;
	vgmstream->next_block_offset = vgmstream->current_block_offset + vgmstream->current_block_size;

	for (i=0;i<vgmstream->channels;i++) 
	{
        vgmstream->ch[i].offset = vgmstream->current_block_offset + (0x2000 * i) + 0x10;
    }
}
