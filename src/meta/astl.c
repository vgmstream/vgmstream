#include "meta.h"
#include "../coding/coding.h"

/* ASTL - found in Dead Rising (PC) */
VGMSTREAM* init_vgmstream_astl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "ASTL"))
        return NULL;
    if (!check_extensions(sf,"ast"))
        return NULL;

    // 04: null
    // 08: 0x201?
    // 0c: version?
    start_offset = read_u32le(0x10,sf);
    // 14: null?
    // 18: null?
    // 1c: null?
    data_size = read_u32le(0x20,sf);
    // 24: -1?
    // 28: -1?
    // 2c: -1?

    if (read_u16le(0x30,sf) != 0x0001) // PCM only
        return NULL;
    channels = read_u16le(0x32, sf);
    int sample_rate = read_s32le(0x34,sf);
    // 38: bitrate
    // 3a: block size
    // 3c: bps

    loop_flag = 0; // unlike X360 no apparent loop info in the files


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ASTL;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
