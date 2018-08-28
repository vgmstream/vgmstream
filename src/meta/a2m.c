#include "meta.h"
#include "../coding/coding.h"

/* A2M - from Artificial Mind & Movement games [Scooby-Doo! Unmasked (PS2)] */
VGMSTREAM * init_vgmstream_a2m(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"int") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41324D00) /* "A2M\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x50533200) /* "PS2\0" */
        goto fail;

    start_offset = 0x30;
    data_size = get_streamfile_size(streamFile) - start_offset;
    channel_count = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_A2M;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x6000;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
