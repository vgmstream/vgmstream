#include "meta.h"

/* LPCM - from Shade's 'Shade game library' (ShdLib) [Ah! My Goddess (PS2), Warship Gunner (PS2)] */
VGMSTREAM* init_vgmstream_lpcm_shade(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint32_t loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00, sf, "LPCM"))
        goto fail;

    /* .w: real extension
     * .lpcm: fake (header id) */
    if (!check_extensions(sf, "w,lpcm"))
        goto fail;

    /* extra checks since header is kind of simple */
    if (read_s32le(0x04,sf) * 0x02 * 2 > get_streamfile_size(sf)) /* data size is less than total samples */
        goto fail;
    if (read_u32le(0x10,sf) != 0) /* just in case */
        goto fail;

    start_offset = 0x800; /* assumed, closer to num_samples */

    loop_flag = read_s32le(0x8,sf) != 0;
    channels = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LPCM_SHADE;
    vgmstream->sample_rate = 48000;

    vgmstream->num_samples = read_s32le(0x4,sf);
    vgmstream->loop_start_sample = read_s32le(0x8,sf);
    vgmstream->loop_end_sample = read_s32le(0xc,sf);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
