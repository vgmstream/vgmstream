#include "meta.h"
#include "../coding/coding.h"

/* XMU - found in Alter Echo (Xbox) */
VGMSTREAM * init_vgmstream_xmu(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    size_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;

    /* check extension */
    if (!check_extensions(streamFile,"xmu"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x584D5520 &&  /* "XMU " */
        read_32bitBE(0x08,streamFile) != 0x46524D54)    /* "FRMT" */
        goto fail;

    start_offset = 0x800;
    channel_count=read_8bit(0x14,streamFile); /* always stereo files */
    loop_flag = read_8bit(0x16,streamFile); /* no Loop found atm */
    data_size = read_32bitLE(0x7FC,streamFile); /* next to "DATA" */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMU;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
