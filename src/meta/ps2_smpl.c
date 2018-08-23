#include "meta.h"
#include "../coding/coding.h"

/* SMPL - from Homura (PS2) */
VGMSTREAM * init_vgmstream_ps2_smpl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t channel_size;

    /* checks*/
    /* .v0: left channel, .v1: right channel
     * .smpl: header id */
    if ( !check_extensions(streamFile,"v0,v1,smpl") )
       goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x534D504C) /* "SMPL" */
        goto fail;

    channel_count = 1;
    loop_flag = (read_32bitLE(0x30,streamFile) != 0); /* .v1 doesn't have loop points */
    start_offset = 0x40;
    channel_size = read_32bitBE(0x0c,streamFile) - 0x10;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size*channel_count, channel_count);
    vgmstream->loop_start_sample = read_32bitLE(0x30,streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PS2_SMPL;
    vgmstream->allow_dual_stereo = 1;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;

    read_string(vgmstream->stream_name,0x10+1, 0x20,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
