#include "meta.h"
#include "../util.h"

/* ZSD - from Dragon Booster (DS) */
VGMSTREAM* init_vgmstream_zsd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag;

    /* checks */
    if (!is_id32be(0x00,sf, "ZSD\0"))
        return NULL;

    /* .zsd: actual extension */
    if (!check_extensions(sf, "zsd"))
        return NULL;

    /* 0x04: 0x1000? */
    /* 0x08: 0x0c? */
    /* 0x14: 0x08? */
    /* 0x1c: 0x1000? */
    channels = read_s32le(0x0c,sf);
    
    loop_flag = 0;
    start_offset = read_s32le(0x20,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ZSD;
    vgmstream->sample_rate = read_s32le(0x10,sf);
    vgmstream->num_samples = read_s32le(0x18,sf) / channels;
    vgmstream->coding_type = coding_PCM8;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
