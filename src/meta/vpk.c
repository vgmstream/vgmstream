#include "meta.h"
#include "../coding/coding.h"

/* VPK - from SCE America second party devs [God of War (PS2), NBA 08 (PS3)] */
VGMSTREAM* init_vgmstream_vpk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels;
    off_t start_offset, loop_channel_offset;


    /* checks */
    if (!is_id32be(0x00,sf, " KPV"))
        return NULL;
    if (!check_extensions(sf, "vpk"))
        return NULL;

    /* files are padded with garbage/silent 0xC00000..00 frames, and channel_size sometimes
     * has extra size into the padding: +0x10 (NBA08), +0x20 (GoW), or none (Sly 2, loops ok).
     * Could detect and remove to slightly improve full loops, but maybe this is just how the game works */
    size_t channel_size = read_u32le(0x04,sf);

    start_offset = read_u32le(0x08,sf);
    channels = read_s32le(0x14,sf);
    /* 0x18+: channel config(?), 0x04 per channel */
    loop_channel_offset = read_u32le(0x7FC,sf);
    loop_flag = (loop_channel_offset != 0); /* found in Sly 2/3 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x10,sf);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size * channels, channels);
    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(loop_channel_offset * channels, channels);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_VPK;
    vgmstream->coding_type = coding_PSX;
    vgmstream->interleave_block_size = read_u32le(0x0C,sf) / 2; /* even in >2ch */
    vgmstream->layout_type = layout_interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
