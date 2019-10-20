#include "meta.h"
#include "../coding/coding.h"

/* BWAV - NintendoWare(?) [Super Mario Maker 2 (Switch)] */
VGMSTREAM * init_vgmstream_bwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, codec;
    size_t interleave = 0;


    /* checks */
    if (!check_extensions(streamFile, "bwav"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x42574156) /* "BWAV" */
        goto fail;

    /* 0x04: BOM */
    /* 0x06: version? */
    /* 0x08: ??? */
    /* 0x0c: null? */
    channel_count = read_16bitLE(0x0E, streamFile);

    /* - per channel (size 0x4c) */
    codec = read_16bitLE(0x10, streamFile);
    /* see below */
    start_offset = read_32bitLE(0x40, streamFile);
    loop_flag = read_32bitLE(0x4C, streamFile) != -1;
    if (channel_count > 1)
        interleave = read_32bitLE(0x8C, streamFile) - start_offset;
    //TODO should make sure channels match and offsets make a proper interleave (see bfwav)


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x14, streamFile);
    vgmstream->num_samples = read_32bitLE(0x18, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x50, streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x4C, streamFile);
    vgmstream->meta_type = meta_BWAV;

    switch(codec) {
        case 0x0000:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            break;

        case 0x0001:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            dsp_read_coefs_le(vgmstream, streamFile, 0x20, 0x4C);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
