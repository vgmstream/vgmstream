#include "layout.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../vgmstream.h"


/* VGS multistream frames */
void block_update_vgs(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    size_t file_size = get_streamfile_size(vgmstream->ch[0].streamfile);
    int i;
    size_t channel_size = 0x10;


    /* set offsets */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + channel_size*i;
    }

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + channel_size*vgmstream->channels;

    /* skip unhandled tracks: flag can be 0x0n per track, of 0x8x for last frame */
    while (vgmstream->next_block_offset < file_size) {
        if ((read_8bit(vgmstream->next_block_offset + 0x01, sf) & 0x0F) == 0x00)
            break;

        vgmstream->next_block_offset += channel_size;
    }

}
