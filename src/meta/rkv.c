#include "meta.h"
#include "../coding/coding.h"

/* RKV - from Legacy of Kain - Blood Omen 2 (PS2) */
VGMSTREAM * init_vgmstream_ps2_rkv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "rkv"))
        goto fail;
    if (read_32bitBE(0x24,streamFile) != 0x00) /* quick test vs GC rkv (coef position) */
        goto fail;

    /* some RKV got info at offset 0x00, some other at 0x0 4 */
    if (read_32bitLE(0x00,streamFile)==0)
        header_offset = 0x04;
    else
        header_offset = 0x00;

    switch (read_32bitLE(header_offset+0x0c,streamFile)) {
        case 0x00: channel_count = 1; break;
        case 0x01: channel_count = 2; break;
        default: goto fail;
    }
    loop_flag = (read_32bitLE(header_offset+0x04,streamFile) != 0xFFFFFFFF);
    start_offset = 0x800;
    data_size = get_streamfile_size(streamFile) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(header_offset,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    //vgmstream->num_samples = read_32bitLE(header_offset+0x08,streamFile); /* sometimes not set */
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(header_offset+0x04,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(header_offset+0x08,streamFile);
    }

    vgmstream->meta_type = meta_PS2_RKV;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* RKV - from Legacy of Kain - Blood Omen 2 (GC) */
VGMSTREAM * init_vgmstream_ngc_rkv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* "": empty (files have names but no extensions), .rkv: container bigfile extension, .bo2: fake extension */
    if (!check_extensions(streamFile, ",rkv,bo2"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x00)
        goto fail;
    if (read_32bitBE(0x24,streamFile) == 0x00) /* quick test vs GC rkv (coef position) */
        goto fail;

    switch (read_32bitBE(0x10,streamFile)) {
        case 0x00: channel_count = 1; break;
        case 0x01: channel_count = 2; break;
        default: goto fail;
    }
    loop_flag = (read_32bitBE(0x08,streamFile) != 0xFFFFFFFF);
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x04,streamFile);
    vgmstream->num_samples = read_32bitBE(0x0C,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x08,streamFile);
        vgmstream->loop_end_sample = read_32bitBE(0x0C,streamFile);
    }

    vgmstream->meta_type = meta_NGC_RKV;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;

    dsp_read_coefs_be(vgmstream,streamFile,0x24,0x2e);
    /* hist at 0x44/0x72? */


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
