#include "meta.h"
#include "../coding/coding.h"

/* GCub - found in Sega Soccer Slam (GC) */
VGMSTREAM* init_vgmstream_gcub(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag, sample_rate;
    size_t data_size;


    /* checks */
    /* .wav: extension found in bigfile
     * .gcub: header id */
    if (!check_extensions(sf, "wav,lwav,gcub"))
        goto fail;
    if (!is_id32be(0x00,sf, "GCub"))
        goto fail;

    loop_flag = 0;
    channels = read_u32be(0x04,sf);
    sample_rate = read_u32be(0x08,sf);
    data_size = read_u32be(0x0c,sf);

    if (is_id32be(0x60,sf, "GCxx")) /* seen in sfx */
        start_offset = 0x88;
    else
        start_offset = 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GCUB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8000;

    dsp_read_coefs_be(vgmstream, sf, 0x10, 0x20);
    /* 0x50: initial ps for ch1/2 (16b) */
    /* 0x54: hist? (always blank) */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
