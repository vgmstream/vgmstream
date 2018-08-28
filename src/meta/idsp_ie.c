#include "meta.h"
#include "../coding/coding.h"


/* IDSP - from Inevitable Entertainment games [Defender (GC)] */
VGMSTREAM * init_vgmstream_idsp_ie(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"idsp") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitBE(0x0C,streamFile);
    if (channel_count > 2) goto fail;
    start_offset = 0x70;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IDSP_IE;
    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = dsp_bytes_to_samples(read_32bitBE(0x04,streamFile), channel_count);

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x10,streamFile);
    dsp_read_coefs_be(vgmstream,streamFile,0x14,0x2E);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
