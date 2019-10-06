#include "meta.h"
#include "../util.h"

/* SAP - from Bubble Symphony (SAT) */
VGMSTREAM * init_vgmstream_sat_sap(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int num_samples;
    int loop_flag = 0, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "sap"))
        goto fail;

    num_samples = read_32bitBE(0x00,streamFile); /* first for I/O reasons */
    channel_count = read_32bitBE(0x04,streamFile);
    if (channel_count != 1) goto fail; /* unknown layout */

    if (read_32bitBE(0x08,streamFile) != 0x10) /* bps? */
        goto fail;
    if (read_16bitBE(0x0c,streamFile) != 0x400E) /* ? */
        goto fail;

    loop_flag = 0;
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SAP;
    vgmstream->sample_rate = (uint16_t)read_16bitBE(0x0E,streamFile);
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
