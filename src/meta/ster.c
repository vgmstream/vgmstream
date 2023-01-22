#include "meta.h"
#include "../coding/coding.h"

/* STER - from Silicon Studios/Vicarious Visions's ALCHEMY middleware [Baroque (PS2), Star Soldier (PS2)] */
VGMSTREAM* init_vgmstream_ster(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t channel_size, loop_start;


    /* checks */
    if (!is_id32be(0x00,sf, "STER"))
        goto fail;

    /* .ster: header id (no apparent names/extensions)
     * .sfs: generic bigfile extension (to be removed?)*/
    if (!check_extensions(sf, "ster,sfs"))
        goto fail;

    channel_size = read_u32le(0x04, sf);
    loop_start = read_u32le(0x08, sf); /* absolute (ex. offset 0x50 for full loops) */
    /* 0x0c: data size BE */
    sample_rate = read_s32be(0x10,sf);
    /* 0x14~20: null */

    loop_flag = loop_start != 0xFFFFFFFF;
    channels = 2; /* mono files are simply .VAG */
    start_offset = 0x30;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STER;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start - start_offset, channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    read_string(vgmstream->stream_name,0x10+1, 0x20,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
