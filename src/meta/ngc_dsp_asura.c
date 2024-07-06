#include "meta.h"
#include "../coding/coding.h"


/* .sfx - Rebellion (Asura engine) games [Sniper Elite (Wii)] */
/* Despite what the extension implies it's used for music too */
/* Rebellion's other DSP variants can be found in ngc_dsp_std */
VGMSTREAM* init_vgmstream_dsp_asura_sfx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, interleave, loop_flag;
    uint32_t nibble_count, sample_rate;
    off_t ch1_offset, ch2_offset;


    /* checks */
    if (!check_extensions(sf, "sfx"))
        return NULL;

    /* no clear header id, but this is how they all start */
    /* the 0x02s are likely channels and codec (DSPADPCM) */
    if (read_u32be(0x00, sf) != 0x00 ||
        read_u32be(0x04, sf) != 0x02 ||
        read_u32be(0x08, sf) != 0x02)
        return NULL;


    nibble_count = read_u32be(0x0C, sf);
    sample_rate = read_u32be(0x10, sf); /* always 44100? */

    /* this would likely be an array, but always 2ch so */
    ch1_offset = read_u32be(0x14, sf); /* always 0x20? */
    ch2_offset = read_u32be(0x18, sf); /* 0x10 aligned */

    interleave = ch2_offset - ch1_offset;

    /* channel header:
     * 0x00: coefs 
     * 0x20: gain (0)
     * 0x22: initial ps
     * 0x30: stream start
     */

    channels = 2;
    loop_flag = 0;

    /* more safety checks */
    if (interleave < 0 || interleave < nibble_count / 2 ||
        interleave > get_streamfile_size(sf) / channels)
        goto fail;

    if (read_u16be(ch1_offset + 0x22, sf) != read_u8(ch1_offset + 0x30, sf) ||
        read_u16be(ch2_offset + 0x22, sf) != read_u8(ch2_offset + 0x30, sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_DSP_ASURA;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs_be(vgmstream, sf, ch1_offset, interleave);
    vgmstream->num_samples = dsp_nibbles_to_samples(nibble_count);

    if (!vgmstream_open_stream(vgmstream, sf, ch1_offset + 0x30))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
