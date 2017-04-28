#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* MPDS - found in Paradigm Entertainment GC games */
VGMSTREAM * init_vgmstream_ngc_dsp_mpds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, short_mpds;


    /* check extension, case insensitive */
    /* .dsp: Big Air Freestyle */
    /* .mds: Terminator 3 The Redemption, Mission Impossible: Operation Surma */
    if (!check_extensions(streamFile, "dsp,mds")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D504453) /* "MPDS" */
        goto fail;

    short_mpds = read_32bitBE(0x04,streamFile) != 0x00010000 && check_extensions(streamFile, "mds"); /* version byte? */

    channel_count = short_mpds ?
            read_16bitBE(0x0a, streamFile) :
            read_32bitBE(0x14, streamFile);
    if (channel_count > 2) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->meta_type = meta_NGC_DSP_MPDS;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;

    if (!short_mpds) { /* Big Air Freestyle */
        start_offset = 0x80;
        vgmstream->num_samples = read_32bitBE(0x08,streamFile);
        vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
        vgmstream->interleave_block_size = channel_count==1 ? 0 : read_32bitBE(0x18,streamFile);

        /* compare sample count with body size */
        if ((vgmstream->num_samples / 7 * 8) != (read_32bitBE(0x0C,streamFile))) goto fail;

        dsp_read_coefs_be(vgmstream,streamFile,0x24, 0x28);
    }
    else { /* Terminator 3 The Redemption, Mission Impossible: Operation Surma */
        start_offset = 0x20;
        vgmstream->num_samples = read_32bitBE(0x04,streamFile);
        vgmstream->sample_rate = (uint16_t)read_16bitBE(0x08,streamFile);
        vgmstream->interleave_block_size = channel_count==1 ? 0 : 0x200;

#if 0   //unknown coeffs/hist, related to data after 0x0c? (only coefs 0..7 seem to be needed)
        {
            off_t offset = 0x0c;
            int i,ch;
            for (ch=0; ch < vgmstream->channels; ch++) {
                for (i=0; i < 16; i++)
                    vgmstream->ch[ch].adpcm_coef[i] = read_16bitBE(offset + i*2);
            }
        }
#endif
    }


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
