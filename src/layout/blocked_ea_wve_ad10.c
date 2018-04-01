#include "layout.h"
#include "../coding/coding.h"


/* EA style blocks, one block per channel when stereo */
void block_update_ea_wve_ad10(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size = 0;
    size_t file_size = get_streamfile_size(streamFile);


    while (block_offset < file_size) {
        uint32_t block_id = read_32bitBE(block_offset+0x00, streamFile);

        block_size = read_32bitBE(block_offset+0x04, streamFile);

        if (block_id == 0x41643130 || block_id == 0x41643131) { /* "Ad10/Ad11" audio block/footer found */
            break;
        }
        /* rest may be "MDEC" video blocks */

        block_offset += block_size;
    }

    /* EOF reads (unsure if this helps) */
    if (block_offset >= file_size) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset + 0x04;
        vgmstream->current_block_size = 0;
        return;
    }

    /* set offsets */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + block_size*i + 0x08;
    }

    vgmstream->current_block_size = block_size - 0x08; /* one block per channel */
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size*vgmstream->channels;
}
