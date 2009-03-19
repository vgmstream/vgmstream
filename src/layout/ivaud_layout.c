#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ivaud_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
	off_t	start_offset;
	off_t	interleave_size;
	int32_t nextFrame=0;
	STREAMFILE *streamFile=vgmstream->ch[0].streamfile;

	vgmstream->current_block_offset = block_offset;

	nextFrame=(read_32bitLE(vgmstream->current_block_offset+0x28,streamFile)<<12)+0x800;

	vgmstream->next_block_offset = vgmstream->current_block_offset + nextFrame;
	vgmstream->current_block_size=read_32bitLE(block_offset+0x24,streamFile)/2;

	start_offset=vgmstream->current_block_offset + 0x800;
	interleave_size=(read_32bitLE(block_offset+0x28,streamFile)<<12)/2;

	for(i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = start_offset + (i*interleave_size);
	}
}
