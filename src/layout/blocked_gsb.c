#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_gsb(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    int block_header_size = 0x20; /*from header*/
    int block_channel_size = 0x8000; /*from header, per channel*/

	vgmstream->current_block_offset = block_offset;
	vgmstream->current_block_size = block_channel_size;
	vgmstream->next_block_offset = vgmstream->current_block_offset
	        + block_header_size
	        + block_channel_size * vgmstream->channels;

	for (i=0;i<vgmstream->channels;i++) {
	    int interleave;
	    int filesize = vgmstream->ch[i].streamfile->get_size(vgmstream->ch[i].streamfile);
	    if (vgmstream->next_block_offset > filesize)
	        interleave = (filesize - vgmstream->current_block_offset - block_header_size) / vgmstream->channels;
	    else
	        interleave = block_channel_size;

        vgmstream->ch[i].offset = vgmstream->current_block_offset
                + block_header_size
                + (interleave*i);
    }
}
