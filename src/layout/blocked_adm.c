#include "layout.h"
#include "../vgmstream.h"

/* blocks of 0x1000 with interleave 0x400 but also smaller last interleave */
void block_update_adm(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i, new_full_block;
    size_t block_size, interleave_size, interleave_data;

    /* no header */
    interleave_size = 0x400;
    interleave_data = 0x400;
    block_size = interleave_size * vgmstream->channels;

    /* every 0x1000 is a full block, signaled by PS-ADPCM flags */
    new_full_block = (read_8bit(block_offset+0x01, streamFile) == 0x06);

    /* try to autodetect usable interleave data size as can be smaller when a discrete block ends (ex. 0x10~0x50, varies with file) */
    if (!new_full_block) {
        off_t next_block_offset = block_offset + block_size;

        while (next_block_offset > block_offset) {
            next_block_offset -= 0x10;

            /* check if unused line (all blocks should only use flags 0x06/0x03/0x02) */
            if (read_32bitLE(next_block_offset, streamFile) == 0x00000000) {
                interleave_data -= 0x10;
            }
            else {
                break;
            }
        }
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_size = interleave_data;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + interleave_size*i;

        //if (new_full_block) { /* blocks are not discrete */
        //    vgmstream->ch[i].adpcm_history1_32 = 0;
        //    vgmstream->ch[i].adpcm_step_index = 0;
        //}
    }
}
