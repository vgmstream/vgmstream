#include "meta.h"
#include "../coding/coding.h"

/* SVG - from High Voltage games [Hunter: The Reckoning - Wayward (PS2)] */
VGMSTREAM * init_vgmstream_svg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size, interleave;
    int loop_flag, channel_count;
    int32_t loop_start = 0, loop_end = 0;


    /* checks */
    if ( !check_extensions(streamFile,"svg") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53564770) /* "SVGp" */
        goto fail;

    start_offset = 0x800;
    data_size = read_32bitLE(0x18,streamFile);
    interleave = read_32bitLE(0x14,streamFile);
    channel_count = 2;
    loop_flag = ps_find_loop_offsets(streamFile, start_offset, data_size, channel_count, interleave,&loop_start, &loop_end);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SVG;
    vgmstream->sample_rate = read_32bitBE(0x2c,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    read_string(vgmstream->stream_name,0x10+1, 0x04,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
