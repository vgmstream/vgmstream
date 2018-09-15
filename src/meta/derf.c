#include "meta.h"
#include "../coding/coding.h"

/* DERF - from Stupid Invaders (PC) */
VGMSTREAM * init_vgmstream_derf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile, "adp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x44455246) /* "DERF" */
        goto fail;

    channel_count = read_32bitLE(0x04,streamFile);
    if (channel_count > 2) goto fail;
    /* movie DERF also exist with slightly different header */

    start_offset = 0x0c;
    data_size = read_32bitLE(0x08,streamFile);
    if (data_size + start_offset != get_streamfile_size(streamFile))
        goto fail;

    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 22050;
    vgmstream->meta_type = meta_DERF;
    vgmstream->coding_type = coding_DERF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->num_samples = data_size / channel_count; /* bytes-to-samples */

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
