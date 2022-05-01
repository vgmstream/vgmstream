#include "layout.h"

/* Traveller's Tales blocks (.AUDIO_DATA) */
void block_update_tt_ad(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    uint32_t header_id, block_size, header_size;
    int i;

    header_size = 0x00;
    block_size = vgmstream->frame_size;

    //TODO could be optimized?
    /* first chunk and last frame has an extra header:
     * 0x00: id
     * 0x04: 0 in FRST, left samples in LAST, others not seen (found in exe) */
    header_id = read_u32be(block_offset, sf);
    if (header_id == get_id32be("FRST") || header_id == get_id32be("LAST") || 
            header_id == get_id32be("LSRT") || header_id == get_id32be("LEND")) {
        header_size = 0x08;
    }
    VGM_ASSERT(header_id == get_id32be("LSRT") || header_id == get_id32be("LEND"), "TT-AD: loop found\n");

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = block_size /* * vgmstream->channels*/;
    vgmstream->next_block_offset = block_offset +  block_size * vgmstream->channels + header_size;

    /* MS-IMA = same offset per channel */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset + header_size + block_size * i;
    }
}
