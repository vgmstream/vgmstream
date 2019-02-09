#include "layout.h"
#include "../vgmstream.h"

/* process headered blocks with sub-headers */
void block_update_mul(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i, block_type;
    size_t block_size, block_header, data_size, data_header;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;

    block_type = read_32bit(block_offset + 0x00,streamFile);
    block_size = read_32bit(block_offset + 0x04,streamFile); /* not including main header */

    switch(vgmstream->coding_type) {
        case coding_NGC_DSP:
            block_header = 0x20;
            data_header  = 0x20;
            break;
        default:
            block_header = 0x10;
            data_header  = 0x10;
            break;
    }

    if (block_type == 0x00 && block_size == 0) {
        /* oddity in some vid+audio files? bad extraction? */
        block_header = 0x10;
        data_header  = 0x00;
        data_size    = 0;
    }
    else if (block_type == 0x00 && block_size != 0) {
        /* read audio sub-header */
        data_size = read_32bit(block_offset + block_header + 0x00,streamFile);
    }
    else if (block_type < 0) {
        /* EOF/bad read */
        data_size = -1;
    }
    else {
        /* non-audio or empty audio block */
        data_header  = 0x00;
        data_size    = 0;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = data_size / vgmstream->channels;
    vgmstream->next_block_offset = block_offset + block_header + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + block_header + data_header + vgmstream->current_block_size*i;
    }
}
