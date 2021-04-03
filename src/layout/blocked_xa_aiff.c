#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* parse a XA AIFF block (CD sector without the 0x18 subheader) */
void block_update_xa_aiff(off_t block_offset, VGMSTREAM* vgmstream) {
    int i;
    size_t block_samples;

    block_samples = (28*8 / vgmstream->channels) * 18; /* size 0x900, 18 frames of size 0x80 with 8 subframes of 28 samples */

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_samples = block_samples;
    vgmstream->next_block_offset = block_offset + 0x914;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset;
    }
}
