#include "meta.h"
#include "../coding/coding.h"
#include "../util/spu_utils.h"


/* VSF - from Square Enix PS2 games between 2004-2006 [Musashi: Samurai Legend (PS2), Front Mission 5 (PS2)] */
VGMSTREAM* init_vgmstream_vsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, flags, pitch;
    size_t channel_size, loop_start;

    /* checks */
    if (!is_id32be(0x00,sf, "VSF\0"))
        return NULL;
    /* .vsf: header id and actual extension [Code Age Commanders (PS2)] */
    if (!check_extensions(sf, "vsf"))
        return NULL;

    // 0x04: data size
    // 0x08: file number?
    // 0x0c: version? (always 0x00010000)
    channel_size = read_u32le(0x10,sf) * 0x10;
    // 0x14: frame size
    loop_start = read_u32le(0x18,sf) * 0x10; // also in channel size
    flags = read_u32le(0x1c,sf);
    pitch = read_s32le(0x20,sf);
    // 0x24: volume?
    // 0x28: ? (may be 0)
    // rest is 0xFF

    channel_count = (flags & (1<<0)) ? 2 : 1;
    loop_flag = (flags & (1<<1));
    start_offset = (flags & (1<<8)) ? 0x80 : 0x800;
    /* flag (1<<4) is common but no apparent differences, no other flags known */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = spu2_pitch_to_sample_rate_rounded(pitch);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, 1);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    vgmstream->meta_type = meta_VSF;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
