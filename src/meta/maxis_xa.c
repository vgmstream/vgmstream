#include "meta.h"
#include "../util.h"

/* Maxis XA - found in Sim City 3000 (PC), The Sims 2 (PC) */
VGMSTREAM * init_vgmstream_maxis_xa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int avg_byte_rate, channel_count, loop_flag, resolution, sample_align, sample_rate;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"xa"))
        goto fail;

    /* check header */
    if ((read_16bitBE(0x00,streamFile) != 0x5841))   /* "XA" */
        goto fail;

    /* check format tag */
    if ((read_16bitLE(0x08,streamFile) != 0x0001))
        goto fail;

    channel_count = read_16bitLE(0x0A,streamFile);
    sample_rate = read_32bitLE(0x0C,streamFile);
    avg_byte_rate = read_32bitLE(0x10,streamFile);
    sample_align = read_16bitLE(0x14,streamFile);
    resolution = read_16bitLE(0x16,streamFile);

    /* check alignment */
    if (sample_align != (resolution/8)*channel_count)
        goto fail;

    /* check average byte rate */
    if (avg_byte_rate != sample_rate*sample_align)
        goto fail;

    loop_flag = 0;
    start_offset = 0x18;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = read_32bitLE(0x04,streamFile)/2/channel_count;

    vgmstream->meta_type = meta_MAXIS_XA;
    vgmstream->coding_type = coding_MAXIS_XA;
    vgmstream->layout_type = layout_none;

    /* open streams */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
