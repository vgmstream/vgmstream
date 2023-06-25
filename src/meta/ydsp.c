#include "meta.h"
#include "../coding/coding.h"


/* YDSP - from Yuke's games [WWE Day of Reckoning (GC), WWE WrestleMania XIX (GC)] */
VGMSTREAM* init_vgmstream_ydsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    uint32_t start_offset;

    /* checks */
    if (!is_id32be(0x00,sf, "YDSP"))
        return NULL;

    /* .ydsp: header id (in bigfile, .yds is the likely extension comparing similar files) */
    if (!check_extensions(sf, "ydsp"))
        return NULL;

    loop_flag = (read_s32be(0xB0,sf) != 0x0);
    channels = read_u16be(0x10,sf);
    start_offset = 0x120;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_YDSP;
    vgmstream->sample_rate = read_s32be(0x0C,sf);

    vgmstream->num_samples = dsp_bytes_to_samples(read_u32be(0x08,sf), channels);
    vgmstream->loop_start_sample = read_s32be(0xB0,sf);
    vgmstream->loop_end_sample = read_s32be(0xB4,sf);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x14,sf);

    dsp_read_coefs_be(vgmstream, sf, 0x20, 0x24);
    //dsp_read_hist_be(vgmstream, sf, 0x20 + 0x20, 0x24);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
