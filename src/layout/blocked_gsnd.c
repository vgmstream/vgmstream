#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_gsnd(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    int block_header_size = 0x20; // from header
    int block_channel_size = 0x8000; // from header, per channel

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_channel_size;
    vgmstream->next_block_offset = block_offset + block_header_size + block_channel_size * vgmstream->channels;

    uint32_t file_size = get_streamfile_size(sf);
    for (int i = 0; i < vgmstream->channels; i++) {
        int interleave;
        if (vgmstream->next_block_offset > file_size)
            interleave = (file_size - block_offset - block_header_size) / vgmstream->channels;
        else
            interleave = block_channel_size;

        vgmstream->ch[i].offset = block_offset + block_header_size + (interleave * i);
    }
}
