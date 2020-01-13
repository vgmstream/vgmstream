#include "meta.h"
#include "../coding/coding.h"

/* NXAP - Nex Entertainment header [Time Crisis 4 (PS3), Time Crisis Razing Storm (PS3)] */
VGMSTREAM * init_vgmstream_nxap(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "adp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E584150) /* "NXAP" */
        goto fail;
    if (read_32bitLE(0x14,streamFile) != 0x40 ||    /* expected frame size? */
        read_32bitLE(0x18,streamFile) != 0x40)      /* expected interleave? */
        goto fail;

    start_offset = read_32bitLE(0x04,streamFile);
    channel_count = read_32bitLE(0x0c,streamFile);
    loop_flag = (read_32bitLE(0x24,streamFile) > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10, streamFile);
    vgmstream->num_samples = read_32bitLE(0x1c,streamFile) * (0x40-0x04)*2 / channel_count; /* number of frames */

    /* in channel blocks, also 0x28/2c values seem related */
    vgmstream->loop_start_sample = read_32bitLE(0x20,streamFile) * (0x40-0x04)*2;
    vgmstream->loop_end_sample = read_32bitLE(0x24,streamFile) * (0x40-0x04)*2;

    vgmstream->meta_type = meta_NXAP;
    vgmstream->coding_type = coding_NXAP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x40;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
