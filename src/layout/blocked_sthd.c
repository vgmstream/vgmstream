#include "layout.h"

/* Dream Factory STHD blocks */
void block_update_sthd(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    size_t block_size, channel_size;
    off_t data_offset;
    int i;

    block_size = 0x800;
    data_offset  = read_16bitLE(block_offset + 0x04, sf);
    channel_size = read_16bitLE(block_offset + 0x16, sf);
    /* 0x06: num channels, 0x10: total blocks, 0x12: block count, 0x14(2): null, 0x18: block count + 1 */

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = channel_size;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + data_offset + channel_size*i;
    }
}
