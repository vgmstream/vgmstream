#include "meta.h"
#include "../coding/coding.h"


/* VBK - from Disney's Stitch - Experiment 626 (PS2) */
VGMSTREAM * init_vgmstream_ps2_vbk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset, stream_offset;
    size_t stream_size, interleave;
    int loop_flag, channel_count, sample_rate;
    int32_t num_samples, loop_start = 0, loop_end = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "vbk"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x2E56424B) /* ".VBK" */
        goto fail;
    /* 0x04: version? always 0x02? */
    start_offset = read_32bitLE(0x0C, streamFile);
    /* 0x10: file size */

    total_subsongs = read_32bitLE(0x08,streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x14 + (target_subsong-1)*0x18;

    stream_size   = read_32bitLE(header_offset+0x00,streamFile);
    /* 0x04: id? */
    stream_offset = read_32bitLE(header_offset+0x08,streamFile);
    sample_rate   = read_32bitLE(header_offset+0x0c,streamFile);
    interleave    = read_32bitLE(header_offset+0x10,streamFile);
    channel_count = read_32bitLE(header_offset+0x14,streamFile) + 1; /* 4ch is common, 1ch sfx too */
    start_offset += stream_offset;

    num_samples = ps_bytes_to_samples(stream_size,channel_count);
    loop_flag = ps_find_loop_offsets(streamFile, start_offset, stream_size, channel_count, interleave, &loop_start, &loop_end);
    loop_flag = loop_flag && (num_samples > 10*sample_rate); /* disable looping for smaller files (in seconds) */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_VBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
