#include "meta.h"
#include "../coding/coding.h"

/* .str - Cauldron/Conan mini-header + interleaved dsp data [Conan (GC)] */
VGMSTREAM* init_vgmstream_ngc_str(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag;


    /* checks */
    if (!check_extensions(sf, "str"))
        goto fail;
    if (read_u32be(0x00,sf) != 0xFAAF0001) /* header id */
        goto fail;

    channels = 2; /* always loop & stereo */
    loop_flag = 1;
    start_offset = 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x04,sf);
    vgmstream->num_samples = read_s32be(0x08,sf);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STR;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_u32be(0x0C,sf);

    dsp_read_coefs_be(vgmstream, sf, 0x10, 0x20);


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
