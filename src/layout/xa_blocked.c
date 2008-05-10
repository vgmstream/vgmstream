#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void xa_block_update(off_t block_offset, VGMSTREAM * vgmstream) {

	int i;
	uint8_t currentChannel=0;
	uint8_t subAudio=0;

	// i used interleave_block_size to check sector read length
	if(vgmstream->samples_into_block!=0)
		// don't change this variable in the init process
		vgmstream->interleave_block_size+=128;

	// We get to the end of a sector ?
	if(vgmstream->interleave_block_size==(18*128)) {
		vgmstream->interleave_block_size=0;

		// 0x30 of unused bytes/sector :(
		block_offset+=0x30;
begin:
		// Search for selected channel & valid audio
		currentChannel=read_8bit(block_offset-7,vgmstream->ch[0].streamfile);
		subAudio=read_8bit(block_offset-6,vgmstream->ch[0].streamfile);

		// audio is coded as 0x64
		if((subAudio!=0x64) || (currentChannel!=vgmstream->xa_channel)) {
			// go to next sector
			block_offset+=2352;
			if(currentChannel!=-1) goto begin;
		} 
	}

	vgmstream->current_block_offset = block_offset;

	// Quid : how to stop the current channel ???
	// i set up 0 to current_block_size to make vgmstream not playing bad samples
	// another way to do it ??? 
	// (as the number of samples can be false in cd-xa due to multi-channels)
	vgmstream->current_block_size = (currentChannel==-1?0:112);
	
	vgmstream->next_block_offset = vgmstream->current_block_offset+128;
	for (i=0;i<vgmstream->channels;i++) {
	    vgmstream->ch[i].offset = vgmstream->current_block_offset;
	}		
}
