#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* MPDS - found in Paradigm Entertainment GC games */
VGMSTREAM* init_vgmstream_mpds(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, short_mpds;


    /* checks */
    if (!is_id32be(0x00,sf, "MPDS"))
        return NULL;
    /* .dsp: Big Air Freestyle */
    /* .mds: Terminator 3 The Redemption, Mission Impossible: Operation Surma */
    if (!check_extensions(sf, "dsp,mds"))
        return NULL;

    /* version byte? */
    short_mpds = read_u32be(0x04,sf) != 0x00010000 && check_extensions(sf, "mds");

    channels = short_mpds ?
            read_u16be(0x0a, sf) :
            read_u32be(0x14, sf);
    if (channels > 2)
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MPDS;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;

    if (!short_mpds) { /* Big Air Freestyle */
        start_offset = 0x80;
        vgmstream->num_samples = read_32bitBE(0x08,sf);
        vgmstream->sample_rate = read_32bitBE(0x10,sf);
        vgmstream->interleave_block_size = channels==1 ? 0 : read_32bitBE(0x18,sf);

        /* compare sample count with body size */
        if ((vgmstream->num_samples / 7 * 8) != (read_32bitBE(0x0C,sf))) goto fail;

        dsp_read_coefs_be(vgmstream,sf,0x24, 0x28);
    }
    else { /* Terminator 3 The Redemption, Mission Impossible: Operation Surma */
        start_offset = 0x20;
        vgmstream->num_samples = read_32bitBE(0x04,sf);
        vgmstream->sample_rate = (uint16_t)read_16bitBE(0x08,sf);
        vgmstream->interleave_block_size = channels==1 ? 0 : 0x200;
        /* some kind of hist after 0x0c? */

        /* set coefs, debugged from the MI:OS ELF (helpfully marked as "sMdsCoefs") */
        static const int16_t coefs[16] = {
                0x0000,0x0000,0x0780,0x0000,0x0e60,0xf980,0x0c40,0xf920,
                0x0f40,0xf880,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
        };

        for (int ch = 0; ch < channels; ch++) {
            for (int i = 0; i < 16; i++) {
                vgmstream->ch[ch].adpcm_coef[i] = coefs[i];
            }
        }
    }


    if ( !vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
