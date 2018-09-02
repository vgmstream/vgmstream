#include "meta.h"
#include "../coding/coding.h"

/* APC - from Cryo games [MegaRace 3 (PC)] */
VGMSTREAM * init_vgmstream_apc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"apc") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4352594F) /* "CRYO" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x5F415043) /* "_APC" */
        goto fail;
  //if (read_32bitBE(0x08,streamFile) != 0x312E3230) /* "1.20" */
  //    goto fail;

    /* 0x14/18: L/R hist sample? */

    start_offset = 0x20;
    data_size = get_streamfile_size(streamFile) - start_offset;
    channel_count = read_32bitLE(0x1c,streamFile) == 0 ? 1 : 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_APC;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_IMA;
    vgmstream->layout_type = layout_none;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
