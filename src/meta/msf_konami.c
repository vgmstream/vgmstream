#include "meta.h"
#include "../coding/coding.h"


/* MSFC - Konami (Armature?) variation [Metal Gear Solid 2 HD (PS3), Metal Gear Solid 3 HD (PS3)] */
VGMSTREAM * init_vgmstream_msf_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    uint32_t codec;
    int loop_flag, channel_count, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile,"msf"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4D534643) /* "MSFC" */
        goto fail;

    start_offset = 0x20;

    codec = read_32bitBE(0x04,streamFile);
    channel_count = read_32bitBE(0x08,streamFile);
    sample_rate = read_32bitBE(0x0c,streamFile);
    data_size = read_32bitBE(0x10,streamFile); /* without header */
    if (data_size + start_offset != get_streamfile_size(streamFile))
        goto fail;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSF_KONAMI;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0x01:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
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
