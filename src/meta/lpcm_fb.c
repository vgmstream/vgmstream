#include "meta.h"
#include "../coding/coding.h"


/* LPCM - French-Bread's DSP [Melty Blood: Type Lumina (Switch)] */
VGMSTREAM* init_vgmstream_lpcm_fb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int loop_flag, channels, sample_rate;
    int32_t num_samples;

    /* checks */
    if (!is_id32be(0x00, sf, "LPCM"))
        goto fail;

    /* .ladpcm: real extension (honest) */
    if (!check_extensions(sf, "ladpcm"))
        goto fail;

    /* 0x04: dsp offset (0x20) */
    if (read_u32le(0x04, sf) != 0x20)
        goto fail;

    num_samples     = read_s32le(0x20, sf);
    /* 0x24: nibbles? */
    sample_rate     = read_s32le(0x28, sf);
    /* 0x2c: 0? */
    /* 0x30: 2? */
    /* 0x34: nibbles? */    
    /* 0x38: 2? */
    if (read_u32le(0x38, sf) != 2)
        goto fail;

    channels = 1;
    loop_flag = 0;

    start_offset    = 0x78; /* could be 0x80 but this is closer to num_samples */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LPCM_FB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    dsp_read_coefs_le(vgmstream, sf, 0x3c, 0);
    /* 0x5c: hist? */


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
