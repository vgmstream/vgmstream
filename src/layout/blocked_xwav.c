#include "layout.h"
#include "../vgmstream.h"

/* pseudo-blocks that must skip last 0x20 every 0x8000 */
void block_update_xwav(off_t block_offset, VGMSTREAM* vgmstream) {
    int i;
    size_t block_size;

    /* no header */
    block_size = vgmstream->channels * 0x10;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_size = block_size / vgmstream->channels;

    if ((vgmstream->next_block_offset - 0x800) > 0
            && ((vgmstream->next_block_offset - 0x800  + 0x20) % 0x8000) == 0) {
        vgmstream->next_block_offset += 0x20;
    }


    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x10*i;
    }
}
