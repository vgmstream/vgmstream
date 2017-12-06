#include "meta.h"

/* IMU - found in Alter Echo (PS2) */
VGMSTREAM * init_vgmstream_ps2_omu(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* check extension */
    if ( !check_extensions(streamFile,"omu") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4F4D5520 &&  /* "OMU " */
        read_32bitBE(0x08,streamFile) != 0x46524D54)    /* "FRMT" */
        goto fail;

    loop_flag = 1;
    channel_count = (int)read_8bit(0x14,streamFile);
    start_offset = 0x40;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = (int32_t)(read_32bitLE(0x3C,streamFile)/(vgmstream->channels*2));
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;
    vgmstream->meta_type = meta_PS2_OMU;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
