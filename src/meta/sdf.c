#include "meta.h"
#include "../coding/coding.h"

/* SDF - from Beyond Reality games [Agent Hugo - Lemoon Twist (PS2)] */
VGMSTREAM * init_vgmstream_sdf_ps2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"sdf") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53444600) /* "SDF\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x03000000) /* version? */
        goto fail;

    start_offset = 0x18;
    data_size = get_streamfile_size(streamFile) - start_offset;
    if (read_32bitLE(0x08,streamFile) != data_size)
        goto fail;

    channel_count = read_32bitLE(0x10,streamFile);
    loop_flag = 0; /* all files have loop flags but simply fade out normally and repeat */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDF_PS2;
    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* SDF - from Beyond Reality games [Gummy Bears Mini Golf (3DS)] */
VGMSTREAM * init_vgmstream_sdf_3ds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"sdf") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53444600) /* "SDF\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x03000000) /* version? */
        goto fail;

    start_offset = 0x78; /* assumed */
    data_size = get_streamfile_size(streamFile) - start_offset;
    if (read_32bitLE(0x08,streamFile) != data_size)
        goto fail;

    channel_count = read_32bitLE(0x14,streamFile);
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDF_3DS;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = data_size / channel_count;
    dsp_read_coefs_le(vgmstream,streamFile,0x1c,0x2e);
    //todo: there be hist around 0x3c

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
