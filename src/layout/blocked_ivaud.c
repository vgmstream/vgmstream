#include "layout.h"
#include "../vgmstream.h"

/* GTA IV blocks */
void block_update_ivaud(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE *streamFile = vgmstream->ch[0].streamfile;
    size_t header_size, block_samples;
    int i;
    off_t seek_info_offset;

    /* base header */
    seek_info_offset = read_32bitLE(block_offset+0x00,streamFile); /*64b */
    /* 0x08(8): seek table offset */
    /* 0x10(8): seek table offset again? */

    /* seek info (per channel) */
    /* 0x00: start entry */
    /* 0x04: number of entries */
    /* 0x08: unknown */
    /* 0x0c: data size */

    /* seek table (per all entries) */
    /* 0x00: start? */
    /* 0x04: end? */


    /* find header size */
    /* can't see a better way to calc, as there may be dummy entries after usable ones
     * (table is max 0x7b8 + seek table offset + 0x800-padded) */
    if (vgmstream->channels > 3)
        header_size = 0x1000;
    else
        header_size = 0x800;

    /* get max data_size as channels may vary slightly (data is padded, hopefully won't create pops) */
    block_samples = 0;
    for(i = 0;i < vgmstream->channels; i++) {
        size_t channel_samples = read_32bitLE(block_offset + seek_info_offset+0x0c + 0x10*i,streamFile);
        if (block_samples < channel_samples)
            block_samples = channel_samples;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + vgmstream->full_block_size;
    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_size = 0;

    for(i = 0; i < vgmstream->channels; i++) {
        /* use seek table's start entry to find channel offset */
        size_t interleave_size = read_32bitLE(block_offset + seek_info_offset+0x00 + 0x10*i,streamFile) * 0x800;
        vgmstream->ch[i].offset = block_offset + header_size + interleave_size;
    }
}
