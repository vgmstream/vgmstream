#include "layout.h"

/* Falcom RIFF blocks (.DEC/DE2) */
void block_update_dec(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    size_t block_size, header_size;
    int i;

    header_size = 0x08;
    block_size = read_32bitLE(block_offset,streamFile);
    /* 0x04: PCM size? */

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_size;
    vgmstream->next_block_offset = block_offset + block_size + header_size;

    /* MSADPCM = same offset per channel */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset + header_size;
    }
}
