#include "meta.h"
#include "../coding/coding.h"

/* MSV - from Sony MultiStream format [Fight Club  (PS2)] */
VGMSTREAM * init_vgmstream_msv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t channel_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"msv") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4D535670) /* "MSVp" */
        goto fail;

    start_offset = 0x30;
    channel_count = 1;
    channel_size = read_32bitBE(0x0c,streamFile);
    loop_flag = 0; /* no looping and last 16 bytes (end frame) are removed, from Sony's docs */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSV;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size,1);

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
