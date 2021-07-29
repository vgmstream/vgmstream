#include "meta.h"
#include "../coding/coding.h"

/* WAF - KID's earlier PC games [ever17 (PC)] (for RLE-compressed WAFs see https://github.com/dsp2003/e17p) */
VGMSTREAM* init_vgmstream_waf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "waf"))
        goto fail;

    if (!is_id32be(0x00,sf, "WAF\0"))
        goto fail;
    if (read_u32le(0x34,sf) + 0x38 != get_streamfile_size(sf))
        goto fail;

    channels = read_u16le(0x06,sf);
    loop_flag = 0;
    start_offset = 0x38;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WAF;
    vgmstream->sample_rate = read_s32le(0x08, sf);

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size = read_u16le(0x10, sf);
    /* 0x04: null?, 0x0c: avg br, 0x12: bps, 0x14: s_p_f, 0x16~34: coefs (a modified RIFF fmt) */
    if (!msadpcm_check_coefs(sf, 0x16))
        goto fail;

    vgmstream->num_samples = msadpcm_bytes_to_samples(read_u32le(0x34,sf), vgmstream->frame_size, channels);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
