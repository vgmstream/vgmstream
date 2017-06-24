#include "meta.h"
#include "../coding/coding.h"

/* XA30 - found in Driver: Parallel Lines (PC) */
VGMSTREAM * init_vgmstream_pc_xa30(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec;
    size_t file_size, data_size;


    /* check extension, case insensitive */
    /* ".xa30" is just the ID, the real filename should be .XA */
    if (!check_extensions(streamFile,"xa,xa30"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58413330) /* "XA30" */
        goto fail;
    if (read_32bitLE(0x04,streamFile) > 2) goto fail; /* extra check to avoid PS2/PC XA30 mixup */


    /* reportedly from XA2WAV those are offset+data from a second stream (not seen) */
    if (read_32bitLE(0x14,streamFile) != 0 || read_32bitLE(0x1c,streamFile) != 0) goto fail;

    loop_flag = 0;
    channel_count = 2; /* 0x04: channels? (always 2 in practice) */

    codec = read_32bitLE(0x0c,streamFile); /* reportedly from XA2WAV (not seen) */
    start_offset = read_32bitLE(0x10,streamFile);

    file_size = get_streamfile_size(streamFile);
    data_size = read_32bitLE(0x18,streamFile);
    if (data_size+start_offset != file_size) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    /* 0x20: always 00016000?,  rest of the header is null */

    vgmstream->meta_type = meta_PC_XA30;

    switch(codec) {
        case 0x01:   /* MS-IMA variation */
            vgmstream->coding_type = coding_REF_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_32bitLE(0x24,streamFile);
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, vgmstream->interleave_block_size, channel_count);
            break;

        case 0x00:   /* PCM? */
        default:
           goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
