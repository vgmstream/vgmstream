#include "meta.h"
#include "../coding/coding.h"

/* VIS - from Konami games [AirForce Delta Strike (PS2) (PS2)] */
VGMSTREAM * init_vgmstream_vis(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"vis") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x56495341) /* "VISA" */
        goto fail;

    start_offset = 0x800;
    data_size = get_streamfile_size(streamFile) - start_offset;

    loop_flag = read_32bitLE(0x18,streamFile);
    channel_count = read_32bitLE(0x20,streamFile); /* assumed */
    /* 0x1c: always 0x10 */
    /* 0x24: always 0x01 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VIS;
    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x0c,streamFile),channel_count);
    vgmstream->loop_end_sample = ps_bytes_to_samples(read_32bitLE(0x10,streamFile),channel_count);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile); /* usually 0x10 or 0x4000 */
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size =
                (data_size % (vgmstream->interleave_block_size*channel_count)) / channel_count;
    read_string(vgmstream->stream_name,0x10+1, 0x28,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
