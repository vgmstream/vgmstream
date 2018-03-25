#include "meta.h"
#include "../util.h"

/* STRM - common Nintendo NDS streaming format */
VGMSTREAM * init_vgmstream_nds_strm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, codec;


    /* checks */
    if (!check_extensions(streamFile, "strm"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x5354524D)    /* "STRM" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0xFFFE0001 &&  /* Old Header Check */
       (read_32bitBE(0x04,streamFile) != 0xFEFF0001))   /* Some newer games have a new flag */
        goto fail;

    if (read_32bitBE(0x10,streamFile) != 0x48454144 && /* "HEAD" */
        read_32bitLE(0x14,streamFile) != 0x50) /* 0x50-sized head is all I've seen */
        goto fail;

    codec = read_8bit(0x18,streamFile);
    loop_flag = read_8bit(0x19,streamFile);
    channel_count = read_8bit(0x1a,streamFile);
    if (channel_count > 2) goto fail;

    start_offset = read_32bitLE(0x28,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x1c,streamFile);
    vgmstream->num_samples = read_32bitLE(0x24,streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x20,streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_STRM;

    switch (codec) {
        case 0x00: /* [Bleach - Dark Souls (DS)] */
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            break;
        case 0x02: /* [SaGa 2 (DS)] */
            vgmstream->coding_type = coding_NDS_IMA;
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x30,streamFile);
    vgmstream->interleave_last_block_size = read_32bitLE(0x38,streamFile);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
