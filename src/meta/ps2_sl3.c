#include "meta.h"
#include "../coding/coding.h"

/* SL3 - Atari Melbourne House games [ Test Drive Unlimited (PS2), Transformers (PS2)] */
VGMSTREAM * init_vgmstream_sl3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;

    /* checks */
    /* .ms: actual extension, sl3: header id */
    if (!check_extensions(streamFile, "ms,sl3"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x534C3300) /* "SL3\0" */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x14,streamFile);
    start_offset = 0x8000; /* also at 0x24? */


	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(streamFile)-start_offset,channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x1C,streamFile);
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x20,streamFile);
    vgmstream->meta_type = meta_SL3;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
