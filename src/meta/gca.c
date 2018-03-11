#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* GCA - Terminal Reality games [Metal Slug Anthology (Wii), BlowOut (GC)] */
VGMSTREAM * init_vgmstream_gca(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "gca"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x47434131) /* "GCA1" */
        goto fail;

    start_offset = 0x40;
    loop_flag = 0;
    channel_count = 1;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x2A,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = dsp_nibbles_to_samples(read_32bitBE(0x26,streamFile));//read_32bitBE(0x26,streamFile)*7/8;

    vgmstream->layout_type = layout_none; /* we have no interleave, so we have no layout */
    vgmstream->meta_type = meta_GCA;

    dsp_read_coefs_be(vgmstream,streamFile,0x04,0x00);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
