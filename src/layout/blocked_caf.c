#include "layout.h"
#include "../vgmstream.h"

/* each block is a new CAF header */
void block_update_caf(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    // 00: "CAF "
    // 04: block size
    // 08: block number
    // 0c: empty
    // 10: channel 1 offset
    // 14: channel 1 size
    // 18: channel 2 offset
    // 1c: channel 2 size
    // 20: loop start
    // 24: loop end (same as last block)
    // 28: DSP header stuff (repeated per block)
    
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + read_u32be(block_offset + 0x04, sf);
    vgmstream->current_block_size = read_u32be(block_offset + 0x14, sf);

    for (int ch = 0; ch < vgmstream->channels; ch++) {
        vgmstream->ch[ch].offset = block_offset + read_u32be(block_offset + 0x10 + 0x08 * ch, sf);

        /* re-read coeffs (though blocks seem to repeat them) */
        for (int i = 0; i < 16; i++) {
            vgmstream->ch[ch].adpcm_coef[i] = read_s16be(block_offset + 0x34 + 0x2c * ch + 0x02 * i, sf);
        }
    }
}
