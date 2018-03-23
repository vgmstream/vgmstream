#include "meta.h"
#include "../coding/coding.h"

/* .str - Cauldron/Conan mini-header + interleaved dsp data [Conan (GC)] */
VGMSTREAM * init_vgmstream_ngc_str(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;


    /* checks */
    if (!check_extensions(streamFile, "str"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0xFAAF0001) /* header id */
        goto fail;

    channel_count = 2; /* always loop & stereo */
    loop_flag = 1;
    start_offset = 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x04,streamFile);
    vgmstream->num_samples = read_32bitBE(0x08,streamFile);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STR;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x0C,streamFile);

    dsp_read_coefs_be(vgmstream, streamFile, 0x10, 0x20);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
