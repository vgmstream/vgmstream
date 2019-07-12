#include "meta.h"
#include "../coding/coding.h"

/* RAD - from Traveller's Tales' Bionicle Heroes */
VGMSTREAM * init_vgmstream_rad(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag = 0;

    /* checks */
    if (!check_extensions(streamFile, "rad"))
        goto fail;

    start_offset  = read_32bitLE(0x00, streamFile);
    channel_count = read_8bit(0x0D, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x04, streamFile);
    vgmstream->num_samples = read_32bitLE(0x08, streamFile);
    vgmstream->meta_type   = meta_RAD;
    vgmstream->layout_type = layout_none;
    vgmstream->coding_type = coding_PCM16LE;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}