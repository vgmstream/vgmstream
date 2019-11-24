#include "meta.h"


/* IVAG - Namco header (from NUS3) [THE iDOLM@STER 2 (PS3), THE iDOLM@STER: Gravure For You! (PS3)] */
VGMSTREAM * init_vgmstream_ivag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;

    int loop_flag = 0;
    int channel_count;

    /* checks */
    /* .ivag: header id (since format can't be found outside NUS3) */
    if (!check_extensions(streamFile, "ivag"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x49564147) /* "IVAG" */
        goto fail;

    /* 0x04: null */
    channel_count = read_32bitBE(0x08, streamFile);
    loop_flag = (read_32bitBE(0x18, streamFile) != 0);

    /* skip VAGp headers per channel (size 0x40) */
    start_offset = 0x40 + (0x40 * channel_count);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IVAG;

    vgmstream->sample_rate = read_32bitBE(0x0C,streamFile);
    vgmstream->num_samples = read_32bitBE(0x10,streamFile);
    vgmstream->loop_start_sample = read_32bitBE(0x14,streamFile);
    vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x1C,streamFile);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
