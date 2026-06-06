#include "meta.h"
#include "../coding/coding.h"


/* VBK - from High Voltage games [Disney's Stitch: Experiment 626 (PS2)] */
VGMSTREAM* init_vgmstream_vbk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, header_offset, stream_offset;
    size_t stream_size, interleave;
    int loop_flag, channels, sample_rate;
    int32_t num_samples, loop_start = 0, loop_end = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf,".VBK"))
        return NULL;
    if (!check_extensions(sf, "vbk"))
        return NULL;

    // 0x04: version? always 0x02?
    total_subsongs  = read_s32le(0x08,sf);
    start_offset    = read_u32le(0x0C,sf);
    // 0x10: file size

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x14 + (target_subsong - 1) * 0x18;

    stream_size     = read_u32le(header_offset+0x00,sf);
    // 0x04: id?
    stream_offset   = read_u32le(header_offset+0x08,sf);
    sample_rate     = read_s32le(header_offset+0x0c,sf);
    interleave      = read_s32le(header_offset+0x10,sf);
    channels        = read_s32le(header_offset+0x14,sf) + 1; // 4ch is common, 1ch sfx too
    start_offset += stream_offset;

    num_samples = ps_bytes_to_samples(stream_size, channels);
    loop_flag = ps_find_loop_offsets(sf, start_offset, stream_size, channels, interleave, &loop_start, &loop_end);
    loop_flag = loop_flag && (num_samples > 10 * sample_rate); // disable looping for smaller files (in seconds)


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
