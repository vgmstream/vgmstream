#include "layout.h"
#include "../vgmstream.h"

/* a simple headerless block with padding; configured in the main header */
void block_update_rws(off_t block_offset, VGMSTREAM* vgmstream) {
    int i;
    size_t block_size;
    size_t interleave;

    /* no header; size is configured in the main header */
    block_size = vgmstream->full_block_size;
    interleave = vgmstream->interleave_block_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + interleave * i;
    }
}
