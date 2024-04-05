#include "layout.h"
#include "../vgmstream.h"

/* VAS - Manhunt 2 PSP VAGs/2AGs blocked audio layout */
void block_update_vas(off_t block_offset, VGMSTREAM* vgmstream) {
    size_t block_size;
    int num_streams;

    /* no headers */
    block_size = 0x40;
    num_streams = vgmstream->num_streams;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + (block_size * num_streams);
    vgmstream->current_block_size = block_size;
    vgmstream->ch[0].offset = block_offset;
}
