#include "layout.h"
#include "../vgmstream.h"


void block_update_str_snds(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    uint32_t block_type, block_subtype, block_size, block_current;
    int i;

    /* EOF reads: signal we have nothing and let the layout fail */
    if (block_offset >= get_streamfile_size(sf)) {
        vgmstream->current_block_samples = -1;
        return;
    }


    block_type = read_u32be(block_offset + 0x00,sf);
    block_size = read_u32be(block_offset + 0x04,sf);

    block_current = 0; /* ignore block by default (other chunks include MPVD + VHDR/FRAM and FILL) */
    if (block_type == 0x534e4453) { /* SNDS */
        block_subtype = read_u32be(block_offset + 0x10,sf); /* SNDS */
        if (block_subtype == 0x53534d50) {
            block_current = read_u32be(block_offset + 0x14, sf) / vgmstream->channels;
        }
    }

    /* seen in Battle Tryst video frames */
    if (block_size % 0x04)
        block_size += 0x04 - (block_size % 0x04);

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_size = block_current;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x18 + i * vgmstream->interleave_block_size;
    }
}
