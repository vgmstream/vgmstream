#include "meta.h"
#include "../coding/coding.h"


/* .ASS - from Dai Senryaku VII: Exceed (PS2) */
VGMSTREAM * init_vgmstream_ps2_ass(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t channel_size, interleave;
    int loop_flag, channel_count, sample_rate;
    int32_t num_samples, loop_start = 0, loop_end = 0;


    /* checks */
    if (!check_extensions(streamFile, "ass"))
        goto fail;

    start_offset = 0x800;
    channel_count = read_32bitLE(0x00,streamFile); /* assumed */
    if (channel_count != 2) goto fail;
    sample_rate = read_32bitLE(0x04,streamFile);
    channel_size = read_32bitLE(0x08,streamFile);
    interleave = read_32bitLE(0x0c,streamFile);
    num_samples = ps_bytes_to_samples(channel_size,1);

    loop_flag = ps_find_loop_offsets(streamFile, start_offset, channel_size*channel_count, channel_count, interleave, &loop_start, &loop_end);
    loop_flag = loop_flag && (num_samples > 10*sample_rate); /* disable looping for smaller files (in seconds) */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_ASS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
