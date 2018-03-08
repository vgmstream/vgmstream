#include "layout.h"
#include "../vgmstream.h"

/* each block is a new CAF header */
void block_update_caf(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i,ch;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + read_32bitBE(block_offset+0x04, streamFile);
    vgmstream->current_block_size = read_32bitBE(block_offset+0x14, streamFile);

    for (ch = 0; ch < vgmstream->channels; ch++) {
        vgmstream->ch[ch].offset = block_offset + read_32bitBE(block_offset+0x10+(0x08*ch), streamFile);

        /* re-read coeffs (though blocks seem to repeat them) */
        for (i = 0; i < 16; i++) {
            vgmstream->ch[ch].adpcm_coef[i] = read_16bitBE(block_offset+0x34 + 0x2c*ch + 0x02*i, streamFile);
        }
    }
}
