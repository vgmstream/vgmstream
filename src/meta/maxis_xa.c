#include "meta.h"
#include "../util.h"

/* Maxis XA - found in Sim City 3000 (PC) */
VGMSTREAM * init_vgmstream_maxis_xa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"xa"))
        goto fail;

    /* check header */
    if ((read_32bitBE(0x00,streamFile) != 0x58414900) && /* "XAI\0" */
        (read_32bitBE(0x00,streamFile) != 0x58414A00))   /* "XAJ\0" (some odd song uses this, no apparent diffs) */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitLE(0x0A,streamFile);
    start_offset = 0x18;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
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
