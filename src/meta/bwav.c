#include "meta.h"
#include "../coding/coding.h"

/* BWAV - NintendoWare(?) [Super Mario Maker 2 (Switch)] */
VGMSTREAM * init_vgmstream_bwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;
    int big_endian;
    size_t interleave = 0;
    int32_t coef_start_offset, coef_spacing;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* checks */
    if (!check_extensions(streamFile, "bwav"))
        goto fail;

    /* BWAV header */
    if (read_32bitBE(0x00, streamFile) != 0x42574156) /* "BWAV" */
        goto fail;

    if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFEFF) { /* BE BOM check */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFFFE) { /* LE BOM check */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    } else {
        goto fail;
    }

	channel_count = 1; //read_8bit(0x0E, streamFile); Needs to be checked
    start_offset = read_32bit(0x40, streamFile);
	loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bit(0x14, streamFile);
    vgmstream->num_samples = read_32bit(0x18, streamFile);
    vgmstream->meta_type = meta_BWAV;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = 0x08; //Maybe half-file? Needs to be checked
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
