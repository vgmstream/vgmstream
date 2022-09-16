#include "meta.h"
#include "../coding/coding.h"

/* BWAV - NintendoWare wavs [Super Mario Maker 2 (Switch)] */
VGMSTREAM* init_vgmstream_bwav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;
    size_t interleave = 0;


    /* checks */
    if (!is_id32be(0x00, sf, "BWAV"))
        goto fail;

    if (!check_extensions(sf, "bwav"))
        goto fail;

    /* 0x04: BOM (always 0xFEFF = LE) */
    /* 0x06: version? (0x0001) */
    /* 0x08: crc32? (supposedly from all channel data without padding) */
    /* 0x0c: prefetch flag */
    channels = read_u16le(0x0e, sf);

    /* - per channel (size 0x4c) */
    codec           = read_u16le(0x10 + 0x00, sf);
    /* 0x02: channel layout (0=L, 1=R, 2=C) */
    sample_rate     = read_s32le(0x10 + 0x04, sf);
    /* 0x08: num_samples for full (non-prefetch) file, same as below if not prefetch */
    num_samples     = read_s32le(0x10 + 0x0c, sf);
    /* 0x10: coefs */
    /* 0x30: offset for full (non-prefetch) file? */
    start_offset    = read_u32le(0x10 + 0x34, sf);
    /* 0x38: flag? (always 1) */
    loop_end        = read_s32le(0x10 + 0x3C, sf);
    loop_start      = read_s32le(0x10 + 0x40, sf);
    /* 0x44: start predictor */
    /* 0x46: hist1 sample */
    /* 0x48: hist2 sample */
    /* 0x4a: null? */

    loop_flag = (loop_end != -1);

    //TODO should make sure channels match and offsets make a proper interleave (see bfwav)
    if (channels > 1)
        interleave = read_u32le(0x8C, sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BWAV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->allow_dual_stereo = 1; /* Animal Crossing: Happy Home Paradise */

    switch(codec) {
        case 0x0000: /* Ring Fit Adventure (Switch) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            break;

        case 0x0001: /* Super Mario Maker 2 (Switch) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            dsp_read_coefs_le(vgmstream, sf, 0x10 + 0x10, 0x4C);
            dsp_read_hist_le(vgmstream, sf,  0x10 + 0x46, 0x4C);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
