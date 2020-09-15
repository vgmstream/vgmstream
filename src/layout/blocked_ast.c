#include "layout.h"
#include "../vgmstream.h"

/* simple headered blocks */
void block_update_ast(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_data, header_size;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;

    /* 0x00: "BLCK", rest: null */
    block_data = read_32bit(block_offset+0x04,streamFile);
    header_size = 0x20;

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_data;
    vgmstream->next_block_offset = block_offset + block_data*vgmstream->channels + header_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + block_data*i;
    }
}
