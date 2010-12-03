#include "layout.h"
#include "../vgmstream.h"
#include "../coding/acm_decoder.h"
#include "../coding/coding.h"

/* set up for the block at the given offset */
void mtaf_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	STREAMFILE *streamFile=vgmstream->ch[0].streamfile;
	
	if(vgmstream->block_count==1) {
again:
		vgmstream->current_block_offset = block_offset;
		vgmstream->block_count = read_32bitLE(block_offset + 0x0c,streamFile)*2;

		if(vgmstream->block_count==0) 
		{
			block_offset+= read_32bitBE(block_offset + 0x04, streamFile);
			goto again;
		}

		vgmstream->current_block_size = 0x100/2;
		block_offset+=0x10;
	} else
		vgmstream->block_count--;

	init_get_high_nibble(vgmstream);
	vgmstream->next_block_offset = block_offset+0x100+0x10;
	//if(read_8bit(block_offset,streamFile)!=vgmstream->xa_channel) 
	//{
	//	mtaf_block_update(vgmstream->next_block_offset,vgmstream);
	//	return;
	//}

	if(block_offset >= 0x6d1e0)
		i=0;

	for (i=0;i<vgmstream->channels;i++) {
		vgmstream->ch[i].adpcm_step_index = read_16bitLE(block_offset + 0x4 + (i * 2), streamFile);
        if (vgmstream->ch[i].adpcm_step_index < 0) vgmstream->ch[i].adpcm_step_index=0;
        if (vgmstream->ch[i].adpcm_step_index> 88) vgmstream->ch[i].adpcm_step_index=88;
		vgmstream->ch[i].adpcm_history1_32 = (int32_t)read_16bitLE(block_offset + 0x8 + (i * 4), streamFile);
		vgmstream->ch[i].offset = block_offset + (i * vgmstream->current_block_size) + 0x10;
	}
}
