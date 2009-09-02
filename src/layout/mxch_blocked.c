#include "layout.h"
#include "../vgmstream.h"

//MxCh blocked layout as used by Lego Island
void mxch_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
	vgmstream->current_block_offset = block_offset;
	vgmstream->next_block_offset = block_offset +
		read_32bitLE(block_offset+4,vgmstream->ch[0].streamfile)+4;
	if ( 0x4d784368!= read_32bitBE(block_offset,vgmstream->ch[0].streamfile))//ignore non-MxCh blocks
	{
		vgmstream->current_block_size = 0;		
	}
	else
	{	
		vgmstream->current_block_size = read_32bitLE(block_offset+18, vgmstream->ch[0].streamfile);
		//not sure if there are stereo files
		for (i=0;i<vgmstream->channels;i++) {
			vgmstream->ch[i].offset = vgmstream->current_block_offset + 22+
				2*i;
		}
	}
	if(vgmstream->next_block_offset > get_streamfile_size(vgmstream->ch[0].streamfile))
	{
		vgmstream->current_block_size = 0;
		//vgmstream->current_block_size = get_streamfile_size(vgmstream->ch[0].streamfile) - block_offset;	
		vgmstream->next_block_offset = block_offset+4;
	}
}
