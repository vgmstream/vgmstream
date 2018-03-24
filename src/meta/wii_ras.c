#include "meta.h"
#include "../coding/coding.h"

/* RAS_ - from Donkey Kong Country Returns (Wii) */
VGMSTREAM * init_vgmstream_wii_ras(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;

    int loop_flag, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "ras"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x5241535F) /* "RAS_" */
        goto fail;

    loop_flag = 0;
    if (read_32bitBE(0x30,streamFile) != 0 ||
        read_32bitBE(0x34,streamFile) != 0 ||
        read_32bitBE(0x38,streamFile) != 0 ||
        read_32bitBE(0x3C,streamFile) != 0) {
        loop_flag = 1;
    }
    channel_count = 2;
    start_offset = read_32bitBE(0x18,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x14,streamFile);
    vgmstream->meta_type = meta_WII_RAS;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x20,streamFile);

    vgmstream->num_samples = read_32bitBE(0x1c,streamFile)/channel_count/8*14;
    if (loop_flag) { /* loop is block + samples into block */
        vgmstream->loop_start_sample = read_32bitBE(0x30,streamFile)*vgmstream->interleave_block_size/8*14 +
                read_32bitBE(0x34,streamFile);
        vgmstream->loop_end_sample = read_32bitBE(0x38,streamFile)*vgmstream->interleave_block_size/8*14 +
                read_32bitBE(0x3C,streamFile);
    }

    dsp_read_coefs_be(vgmstream,streamFile,0x40,0x30);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
