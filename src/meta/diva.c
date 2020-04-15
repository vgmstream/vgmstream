#include "meta.h"
#include "../coding/coding.h"

/* DIVA - Hatsune Miku: Project DIVA Arcade */
VGMSTREAM * init_vgmstream_diva(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count;
    int loop_end;
    int loop_flag;

    /* checks */
    if (!check_extensions(streamFile, "diva"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x44495641) /* "DIVA" */
        goto fail;

    start_offset  = 0x40;
    channel_count = read_8bit(0x1C, streamFile);
    loop_end      = read_32bitLE(0x18, streamFile);

    loop_flag = (loop_end != 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate       = read_32bitLE(0x0C, streamFile);
    vgmstream->num_samples       = read_32bitLE(0x10, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile);
    vgmstream->loop_end_sample   = loop_end;
    vgmstream->meta_type         = meta_DIVA;
    vgmstream->layout_type       = layout_none;
    vgmstream->coding_type       = coding_DVI_IMA;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}