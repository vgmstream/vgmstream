#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_ea_swvr(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_32bit(vgmstream->current_block_offset+0x04,streamFile)-0x1C;
    vgmstream->next_block_offset = vgmstream->current_block_offset+vgmstream->current_block_size+0x1C;
    vgmstream->current_block_size/=vgmstream->channels;

    for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset+0x1C+(vgmstream->current_block_size*i);
    }
}
