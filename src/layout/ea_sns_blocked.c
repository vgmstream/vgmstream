#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* EA "SNS "blocks (most common in .SNS) */
void ea_sns_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    uint32_t block_size, block_samples;
    size_t file_size = get_streamfile_size(streamFile);
    int i;

    /* always BE */
    block_size    = read_32bitBE(block_offset + 0x00,streamFile);
    block_samples = read_32bitBE(block_offset + 0x04,streamFile);

    /* EOF */
    if (block_size == 0 || block_offset >= file_size) {
        vgmstream->current_block_offset = file_size;
        vgmstream->next_block_offset = file_size + 0x04;
        vgmstream->current_block_samples = vgmstream->num_samples;
        return;
    }

    /* known: 0x80 = last block, 0x40, 0x08, 0x04, 0x01 */
    if (block_size & 0xFF000000) {
        VGM_ASSERT(!(block_size & 0x80000000), "EA SNS: unknown flag found at %lx\n", block_offset);
        block_size &= 0x00FFFFFF;
    }

    for (i = 0; i < vgmstream->channels; i++) {
        off_t channel_start = 0x00;
        vgmstream->ch[i].offset = block_offset + 0x08 + channel_start;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;
}
