#include "meta.h"
#include "../util.h"

/* .SVAG - from SNK games [World Heroes Anthology (PS2), Fatal Fury Battle Archives 2 (PS2)] */
VGMSTREAM* init_vgmstream_svag_snk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, loop_start_block, loop_end_block;

    /* checks */
    if (!check_extensions(sf, "svag"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x5641476D) /* "VAGm" */
        goto fail;

    channel_count = read_32bitLE(0x0c,sf);
    loop_start_block = read_32bitLE(0x18,sf);
    loop_end_block = read_32bitLE(0x1c,sf);
    loop_flag = loop_end_block > 0; /* loop_start_block can be 0 */
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SVAG_SNK;

    vgmstream->sample_rate = read_32bitLE(0x08,sf);
    vgmstream->num_samples = read_32bitLE(0x10,sf) * 28; /* size in blocks */
    vgmstream->loop_start_sample = loop_start_block * 28;
    vgmstream->loop_end_sample = loop_end_block * 28;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
