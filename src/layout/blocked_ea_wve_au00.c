#include "layout.h"
#include "../coding/coding.h"


/* EA style blocks */
void block_update_ea_wve_au00(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size = 0, interleave;
    size_t file_size = get_streamfile_size(streamFile);


    while (block_offset < file_size) {
        uint32_t block_id = read_32bitBE(block_offset+0x00, streamFile);

        block_size = read_32bitBE(block_offset+0x04, streamFile);

        if (block_id == 0x61753030 || block_id == 0x61753031) { /* "au00/au01" audio block/footer found */
            break;
        }
        /* rest may be "MDEC" video blocks */

        block_offset += block_size;
    }

    /* size adjusted to frame boundaries as blocks have padding */
    interleave = ((block_size - 0x10) / vgmstream->interleave_block_size * vgmstream->interleave_block_size) / vgmstream->channels;

    /* set offsets */
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = (block_offset + 0x10) + interleave*i;
    }

    vgmstream->current_block_size = interleave;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
}
