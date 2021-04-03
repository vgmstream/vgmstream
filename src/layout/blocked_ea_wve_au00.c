#include "layout.h"
#include "../coding/coding.h"


/* EA style blocks */
void block_update_ea_wve_au00(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, channel_size = 0;
    uint32_t block_id;

    block_id   = read_32bitBE(block_offset+0x00, sf);
    block_size = read_32bitBE(block_offset+0x04, sf);

    /* accept "au00/au01" audio block/footer */
    if (block_id == 0x61753030 || block_id == 0x61753031) {
        /* adjusted to frame boundaries as blocks have padding */
        channel_size = ((block_size - 0x10) / vgmstream->interleave_block_size * vgmstream->interleave_block_size) / vgmstream->channels;
    }
    /* rest could be "MDEC" video blocks with 0 size/samples */

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = (block_offset + 0x10) + channel_size*i;
    }
}
