#include "meta.h"
#include "../coding/coding.h"


/* KA1A - Koei Tecmo's custom codec streams [Dynasty Warriors Origins (PC)] */
VGMSTREAM* init_vgmstream_ka1a(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset; 

    /* checks */
    if (!is_id32be(0x00,sf, "KA1A"))
        return NULL;
    /* .ka1a: header id */
    if (!check_extensions(sf,"ka1a"))
        return NULL;
    // KA1A don't seem found outside SRST, but probably will (like KOVS)

    //uint32_t data_size = read_u32le(0x04,sf);
    int channels = read_s32le(0x08,sf);
    int tracks = read_s32le(0x0c,sf);
    int sample_rate = read_s32le(0x10,sf);
    int32_t num_samples = read_s32le(0x14,sf);
    int32_t loop_start = read_s32le(0x18,sf);
    int32_t loop_region = read_s32le(0x1c,sf);
    int bitrate_mode = read_s32le(0x20,sf); // signed! (may be negative)
    // 0x28: reserved?

    bool loop_flag = (loop_region > 0);

    start_offset = 0x28;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels * tracks, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KA1A;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_start + loop_region; //typically num_samples

    // KA1A interleaves tracks (ex. 2ch and 2 tracks = 512 stereo samples + 512 stereo samples).
    // For vgmstream this is reinterpreted as plain channels like other KT formats do (codec handles
    // this fine). Encoder delay is implicit.
    vgmstream->codec_data = init_ka1a(bitrate_mode, channels * tracks);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_KA1A;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
