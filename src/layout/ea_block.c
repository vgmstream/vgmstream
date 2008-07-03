#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ea_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;

	// Search for next SCDL or SCEl block ...
	do {
		block_offset+=4;
		if(block_offset>(off_t)get_streamfile_size(vgmstream->ch[0].streamfile))
			return;
	} while (read_32bitBE(block_offset,vgmstream->ch[0].streamfile)!=0x5343446C);

	// reset channel offset
	for(i=0;i<vgmstream->channels;i++) {
		vgmstream->ch[i].channel_start_offset=0;
	}

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset+read_32bitLE(block_offset+4,vgmstream->ch[0].streamfile)-4;

	if(vgmstream->ea_big_endian) {
		vgmstream->current_block_size = read_32bitBE(block_offset+8,vgmstream->ch[0].streamfile);
		
		for(i=0;i<vgmstream->channels;i++) {
			vgmstream->ch[i].offset=read_32bitBE(block_offset+0x0C+(i*4),vgmstream->ch[0].streamfile)+(4*vgmstream->channels);
			vgmstream->ch[i].offset+=vgmstream->current_block_offset+0x0C;
		}
		vgmstream->current_block_size /= 28;

	} else {
		if(vgmstream->coding_type==coding_PSX) {
			vgmstream->ch[0].offset=vgmstream->current_block_offset+0x10;
			vgmstream->ch[1].offset=(read_32bitLE(block_offset+0x04,vgmstream->ch[0].streamfile)-0x10)/vgmstream->channels;
			vgmstream->ch[1].offset+=vgmstream->ch[0].offset;
			vgmstream->current_block_size=read_32bitLE(block_offset+0x04,vgmstream->ch[0].streamfile)-0x10;
			vgmstream->current_block_size/=vgmstream->channels;
		}  else {
			vgmstream->current_block_size = read_32bitLE(block_offset+8,vgmstream->ch[0].streamfile);
			for(i=0;i<vgmstream->channels;i++) {
				vgmstream->ch[i].offset=read_32bitLE(block_offset+0x0C+(i*4),vgmstream->ch[0].streamfile)+(4*vgmstream->channels);
				vgmstream->ch[i].offset+=vgmstream->current_block_offset+0x0C;
			}
			vgmstream->current_block_size /= 28;
		}
	}

	if((vgmstream->ea_compression_version<3) && (vgmstream->coding_type!=coding_PSX)) {
		for(i=0;i<vgmstream->channels;i++) {
			if(vgmstream->ea_big_endian) {
				vgmstream->ch[i].adpcm_history1_32=read_16bitBE(vgmstream->ch[i].offset,vgmstream->ch[0].streamfile);
				vgmstream->ch[i].adpcm_history2_32=read_16bitBE(vgmstream->ch[i].offset+2,vgmstream->ch[0].streamfile);
			} else {
				vgmstream->ch[i].adpcm_history1_32=read_16bitLE(vgmstream->ch[i].offset,vgmstream->ch[0].streamfile);
				vgmstream->ch[i].adpcm_history2_32=read_16bitLE(vgmstream->ch[i].offset+2,vgmstream->ch[0].streamfile);
			}
			vgmstream->ch[i].offset+=4;
		}
	}
}
