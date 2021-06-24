#include "meta.h"
#include "../coding/coding.h"


/* .WVS - found in Metal Arms - Glitch in the System (Xbox) */
VGMSTREAM* init_vgmstream_wvs_xbox(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(sf,"wvs"))
        goto fail;

    data_size = read_u32le(0x00,sf);
    /* 0x04: float seconds (slightly bigger than max num_samples) */
    sample_rate = read_f32le(0x08,sf);
    if (read_u16le(0x0c,sf) != 0x0069)  /* codec */
        goto fail;
    channels = read_s16le(0x0e,sf);
    sample_rate = read_s32le(0x10,sf);
    /* 0x10: sample rate (int) */
    /* 0x14: bitrate */
    /* 0x18: block size / bps */
    /* 0x1c: size? / block samples */

    loop_flag = (channels > 1 && sample_rate >= 44100); /* bgm full loops */
    start_offset = 0x20;

    if (data_size + start_offset != get_streamfile_size(sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WVS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .WVS - found in Metal Arms - Glitch in the System (GC) */
VGMSTREAM* init_vgmstream_wvs_ngc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave;
    size_t data_size;


    /* checks */
    if (!check_extensions(sf,"wvs"))
        goto fail;

    channels = read_s32be(0x00,sf);
    /* 0x04: float seconds (slightly bigger than max num_samples) */
    sample_rate = read_f32be(0x08,sf);
    interleave = read_u32be(0x0C,sf); /* even in mono */
    /* 0x10: number of interleave blocks */
    data_size  = read_s32be(0x14,sf) * channels;

    loop_flag = (channels > 1 && sample_rate >= 44100); /* bgm full loops */
    start_offset = 0x60;

    if (data_size + start_offset != get_streamfile_size(sf))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WVS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    if (interleave)
        vgmstream->interleave_last_block_size = (data_size % (interleave * channels)) / channels;

    dsp_read_coefs_be(vgmstream, sf, 0x18, 0x20);
    //dsp_read_hist_be(vgmstream, sf, 0x18 + 0x20*channels, 0x04); /* not seen */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
