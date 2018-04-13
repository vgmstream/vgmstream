#include "layout.h"
#include "../coding/coding.h"


/* EA style blocks, one block per channel when stereo */
void block_update_ea_wve_ad10(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, channel_size = 0, interleave = 0;
    uint32_t block_id;

    block_id   = read_32bitBE(block_offset+0x00, streamFile);
    block_size = read_32bitBE(block_offset+0x04, streamFile);

    /* accept "Ad10/Ad11" audio block/footer */
    if (block_id == 0x41643130 || block_id == 0x41643131) {
        channel_size = block_size - 0x08; /* one block per channel */
        interleave = block_size;
        block_size = block_size*vgmstream->channels;
    }
    /* rest could be "MDEC" video blocks with 0 size/samples */

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = (block_offset + 0x08) + interleave*i;
    }
}
