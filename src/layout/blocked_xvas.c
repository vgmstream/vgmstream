#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_vas_kceo(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    // last block is smaller in a few non-looped files
    int file_size = get_streamfile_size(sf);
    int block_size = 0x20000;
    if (block_offset + block_size > file_size) {
        block_size = file_size - block_offset;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = (block_size - 0x20) / vgmstream->channels;
    vgmstream->next_block_offset = block_offset + block_size;

    for (int i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset; //stereo XBOX-IMA
    }
}
