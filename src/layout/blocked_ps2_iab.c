#include "layout.h"
#include "../vgmstream.h"

/* blocks with mini header (0x48124812 + unknown + block data + block size) */
void block_update_ps2_iab(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, channel_size;

    channel_size = read_32bitLE(block_offset+0x08,streamFile) / vgmstream->channels;
    block_size = read_32bitLE(block_offset+0x0c,streamFile);
    if (!block_size)
        block_size = 0x10; /* happens on last block */

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x10 + channel_size*i;
    }
}
