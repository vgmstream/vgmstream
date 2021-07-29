#include "meta.h"
#include "../coding/coding.h"

/* RKV - from Legacy of Kain - Blood Omen 2 (PS2) */
VGMSTREAM* init_vgmstream_ps2_rkv(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t data_size;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "rkv"))
        goto fail;
    if (read_u32be(0x24,sf) != 0x00) /* quick test vs GC rkv (coef position) */
        goto fail;

    /* some RKV got info at offset 0x00, some other at 0x0 4 */
    if (read_u32le(0x00,sf) == 0)
        header_offset = 0x04;
    else
        header_offset = 0x00;

    switch (read_u32le(header_offset+0x0c,sf)) {
        case 0x00: channels = 1; break;
        case 0x01: channels = 2; break;
        default: goto fail;
    }
    loop_flag = (read_u32le(header_offset+0x04,sf) != 0xFFFFFFFF);
    start_offset = 0x800;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(header_offset,sf);
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    //vgmstream->num_samples = read_32bitLE(header_offset+0x08,sf); /* sometimes not set */
    if (loop_flag) {
        vgmstream->loop_start_sample = read_s32le(header_offset+0x04,sf);
        vgmstream->loop_end_sample = read_s32le(header_offset+0x08,sf);
    }

    vgmstream->meta_type = meta_PS2_RKV;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* RKV - from Legacy of Kain - Blood Omen 2 (GC) */
VGMSTREAM* init_vgmstream_ngc_rkv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    /* "": empty (files have names but no extensions)
     * .rkv: container bigfile extension
     * .bo2: fake extension */
    if (!check_extensions(sf, ",rkv,bo2"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x00)
        goto fail;
    if (read_u32be(0x24,sf) == 0x00) /* quick test vs GC rkv (coef position) */
        goto fail;

    switch (read_u32be(0x10,sf)) {
        case 0x00: channels = 1; break;
        case 0x01: channels = 2; break;
        default: goto fail;
    }
    loop_flag = (read_u32be(0x08,sf) != 0xFFFFFFFF);
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x04,sf);
    vgmstream->num_samples = read_s32be(0x0C,sf);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_s32be(0x08,sf);
        vgmstream->loop_end_sample = read_s32be(0x0C,sf);
    }

    vgmstream->meta_type = meta_NGC_RKV;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;

    dsp_read_coefs_be(vgmstream,sf,0x24,0x2e);
    /* hist at 0x44/0x72? */


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
