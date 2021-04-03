#include "layout.h"
#include "../vgmstream.h"

/* a simple headerless block with special adpcm history handling */
void block_update_hwas(off_t block_offset, VGMSTREAM* vgmstream) {
    int i;
    size_t block_size;

    /* no header */
    block_size = vgmstream->full_block_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset
              + block_size;
    vgmstream->current_block_size = block_size;

    /* reset ADPCM history every block (no header with hist or anything) */
    /* probably not 100% exact but good enough to get something decently playable (otherwise there are wild volume swings) */
    for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].adpcm_history1_32 = 0;
        vgmstream->ch[i].adpcm_step_index = 0;
        vgmstream->ch[i].offset = block_offset;
    }
}
