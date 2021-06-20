#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .ADS - from Gauntlet Dark Legacy (GC/Xbox) */
VGMSTREAM* init_vgmstream_ads_midway(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, codec;


    /* checks */
    if (!check_extensions(sf,"ads"))
        goto fail;

    /* fake PS2 .ads but BE */
    if (!is_id32be(0x00,sf, "dhSS"))
        goto fail;
    if (!is_id32be(0x20,sf, "dbSS"))
        goto fail;

    loop_flag = 1;
    channels = read_32bitBE(0x10,sf);
    if (channels > 2)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x0c,sf);

    codec = read_32bitBE(0x08,sf);
    switch (codec) {
        case 0x00000020: /* GC */
            start_offset = 0x28 + 0x60 * channels;
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->num_samples = read_32bitBE(0x28,sf);
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
        
            if (channels == 1) {
                vgmstream->layout_type = layout_none;
            } else if (channels == 2) {
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = read_32bitBE(0x14,sf);
            }

            dsp_read_coefs_be(vgmstream, sf, 0x44,0x60);
            break;

        case 0x00000021: /* Xbox */
            start_offset = 0x28;
            vgmstream->coding_type = coding_XBOX_IMA_int;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(read_32bitBE(0x24,sf), vgmstream->channels);
            vgmstream->layout_type = channels == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x24;
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            break;

        default:
            goto fail;
    }

    vgmstream->meta_type = meta_ADS_MIDWAY;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
