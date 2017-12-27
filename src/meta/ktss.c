#include "meta.h"
#include "../util.h"
#include "../stack_alloc.h"
#include "../coding/coding.h"

VGMSTREAM * init_vgmstream_ktss(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    int32_t loop_length;
    off_t start_offset;

    if (!check_extensions(streamFile, "ktss"))
        goto fail;

    if (read_32bitBE(0, streamFile) != 0x4B545353) /* "KTSS" */
        goto fail;

    loop_length = read_32bitLE(0x38, streamFile);
    loop_flag = loop_length > 0;
    channel_count = read_8bit(0x29, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x30, streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x2c, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x34, streamFile);
    vgmstream->loop_end_sample = vgmstream->loop_start_sample + loop_length;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_KTSS;

    vgmstream->interleave_block_size = 0x8;

    dsp_read_coefs_le(vgmstream, streamFile, 0x40, 0x2e);
    start_offset = read_32bitLE(0x24, streamFile) + 0x20;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

