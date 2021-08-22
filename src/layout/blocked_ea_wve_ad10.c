#include "layout.h"
#include "../coding/coding.h"


/* EA style blocks, one block per channel when stereo */
void block_update_ea_wve_ad10(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i;
    size_t channel_size = 0, interleave = 0;
    uint32_t block_id, block_size;

    int flag_be = (vgmstream->codec_config & 0x01);
    uint32_t (*read_u32)(off_t,STREAMFILE*) = flag_be ? read_u32be : read_u32le;

    block_id   = read_u32be(block_offset+0x00, sf);
    block_size = read_u32  (block_offset+0x04, sf);

    /* accept "Ad10/Ad11" audio block/footer */
    if (block_id == 0x41643130 || block_id == 0x41643131) {
        channel_size = block_size - 0x08; /* one block per channel */
        interleave = block_size;
        block_size = block_size * vgmstream->channels;
    }
    /* rest could be "MDEC" video blocks with 0 size/samples */

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = (block_offset + 0x08) + interleave*i;
    }
}
