#include "meta.h"
#include "../coding/coding.h"

/* 04SW - Reflections games [Driver: Parallel Lines (Wii), Emergency Heroes (Wii)] */
VGMSTREAM* init_vgmstream_xa_04sw(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    int32_t num_samples;
    uint32_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "04SW"))
        goto fail;

    if (!check_extensions(sf,"xa"))
        goto fail;

    /* after the ID goes a modified DSP header x2 */
    if (read_u32be(0x04 + 0x0c,sf) != 0) /* should be non looping */
        goto fail;
    loop_flag = 0;
    /* not in header it seems, so just dual header check */
    
    num_samples = read_s32be(0x04 + 0x00,sf);
    data_size = read_u32be(0x04 + 0x04,sf);
    sample_rate = read_u32be(0x0c,sf);

    channels = (read_u32be(0x04 + 0x00,sf) == read_u32be(0x04 + 0x60,sf)) ? 2 : 1; /* some voice .xa */

    /* After DSP header goes a base header with mostly unknown values (several repeats) and the filename.
     * Emergency Heroes has extra 0x10 at 0x10. */
    start_offset = read_u32be(0x04 + 0x60 * 2 + 0x00, sf);
    /* 0x04: data size (includes padding after DSP data in Driver, doesn't in Emergency Heroes */
    /* 0x1c/2c: channels LE? */
    /* 0x74/84: utf-16 path+filename */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channels == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = 0x8000;
    vgmstream->interleave_last_block_size = (data_size / 2 % vgmstream->interleave_block_size + 7) / 8 * 8;

    dsp_read_coefs_be(vgmstream, sf, 0x04 + 0x1c, 0x60);
    /* initial history offset seems different than standard DSP and possibly fixed/invalid */

    vgmstream->meta_type = meta_XA_04SW;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
