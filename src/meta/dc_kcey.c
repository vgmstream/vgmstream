#include "meta.h"

/* KCEY - from Konami KCE Yokohama DC games (Pop'n Music series) */
VGMSTREAM* init_vgmstream_kcey(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "KCEY") || !is_id32be(0x04,sf, "COMP"))
        return NULL;
    /* .pcm: original
     * .kcey: renamed to header id */
    if ( !check_extensions(sf,"pcm,kcey") )
        return NULL;

    start_offset = read_u32be(0x10,sf);
    loop_flag = (read_u32be(0x14,sf) != 0xFFFFFFFF);
    channels = read_s32be(0x08,sf);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 37800;
    vgmstream->num_samples = read_s32be(0x0C,sf);
    vgmstream->loop_start_sample = read_s32be(0x14,sf);
    vgmstream->loop_end_sample = read_s32be(0x0C,sf);

    vgmstream->coding_type = coding_DVI_IMA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_KCEY;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
