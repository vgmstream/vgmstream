#include "meta.h"
#include "../coding/coding.h"

/* VGS - from Princess Soft games [Gin no Eclipse (PS2), Metal Wolf REV (PS2)] */
VGMSTREAM * init_vgmstream_ps2_vgs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size, channel_size, interleave;
    int loop_flag, channel_count;
    int32_t loop_start = 0, loop_end = 0;


    /* check */
    if ( !check_extensions(streamFile,"vgs") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x56475300) /* "VGS\0" ('VAG stereo', presumably) */
        goto fail;

    start_offset = 0x30;
    data_size = get_streamfile_size(streamFile) - start_offset;
    interleave = 0x20000;
    channel_count = 2;
    channel_size = read_32bitBE(0x0c,streamFile);
    loop_flag = 0; /* all files have loop flags but simply fade out normally and repeat */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_VGS;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size,1);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;
    read_string(vgmstream->stream_name,0x10+1, 0x20,streamFile); /* always, can be null */

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
