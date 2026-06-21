#include "meta.h"
#include "../coding/coding.h"

/* NXAP - Nex Entertainment header [Time Crisis 4 (PS3), Time Crisis Razing Storm (PS3)] */
VGMSTREAM* init_vgmstream_nxap(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "NXAP"))
        return NULL;
    if (!check_extensions(sf, "adp"))
        return NULL;
    if (read_u32le(0x14,sf) != 0x40 ||    // expected frame size?
        read_u32le(0x18,sf) != 0x40)      // expected interleave?
        return NULL;

    off_t start_offset = read_u32le(0x04,sf);
    int channels = read_u32le(0x0c,sf);
    int loop_flag = (read_s32le(0x24,sf) > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x10, sf);
    vgmstream->num_samples = read_s32le(0x1c,sf) * (0x40-0x04) * 2 / channels; // number of frames

    // in channel blocks, also 0x28/2c values seem related
    vgmstream->loop_start_sample = read_s32le(0x20,sf) * (0x40 - 0x04) * 2;
    vgmstream->loop_end_sample = read_s32le(0x24,sf) * (0x40 - 0x04) * 2;

    vgmstream->meta_type = meta_NXAP;
    vgmstream->coding_type = coding_NXAP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x40;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
