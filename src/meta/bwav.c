#include "meta.h"
#include "../coding/coding.h"

/* BWAV - NintendoWare(?) [Super Mario Maker 2 (Switch)] */
VGMSTREAM * init_vgmstream_bwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;
    int32_t coef_start_offset, coef_spacing;

    /* checks */
    if (!check_extensions(streamFile, "bwav"))
        goto fail;

    /* BWAV header */
    if (read_32bitBE(0x00, streamFile) != 0x42574156) /* "BWAV" */
        goto fail;

    channel_count = read_16bitLE(0x0E, streamFile);
    start_offset = read_32bitLE(0x40, streamFile);
    loop_flag = read_32bitLE(0x4C, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x14, streamFile);
    vgmstream->num_samples = read_32bitLE(0x18, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x50, streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x4C, streamFile);
    vgmstream->meta_type = meta_BWAV;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x8C, streamFile) - start_offset;
    vgmstream->coding_type = coding_NGC_DSP;
    
    coef_start_offset = 0x20;
    coef_spacing = 0x4C;
    dsp_read_coefs_le(vgmstream, streamFile, coef_start_offset, coef_spacing);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
