#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_ws_aud(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    uint16_t block_size = read_u16le(block_offset + 0x00, sf);
    uint16_t pcm_size   = read_u16le(block_offset + 0x02, sf); // typically 0x800 except in last block

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_size;
    vgmstream->next_block_offset = block_offset + block_size + 0x08;

    if (vgmstream->coding_type == coding_WS) {
        vgmstream->ws_output_size = pcm_size;
    }

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x08 + block_size * i;
    }
}
