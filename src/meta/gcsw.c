#include "meta.h"


/* GCSW - from Radirgy GeneriC (GC) */
VGMSTREAM * init_vgmstream_gcsw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag;
    off_t start_offset;


    /* checks */
    if (!check_extensions(streamFile, "gcw"))
        goto fail;

    if (read_32bitBE(0,streamFile) != 0x47435357) /* "GCSW" */
        goto fail;

    start_offset = 0x20;
    channel_count = read_32bitBE(0x0c,streamFile);
    loop_flag = read_32bitBE(0x1c,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GCSW;

    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->num_samples = read_32bitBE(0x10,streamFile);
    vgmstream->loop_start_sample = read_32bitBE(0x14,streamFile);
    vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile);

    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8000;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
