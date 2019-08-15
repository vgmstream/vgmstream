#include "meta.h"
#include "../coding/coding.h"


/* .IMA - Blitz Games early games [Lilo & Stitch: Trouble in Paradise (PC)] */
VGMSTREAM * init_vgmstream_ima(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, num_samples, sample_rate;


    /* checks */
    if (!check_extensions(streamFile, "ima"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x02000000) /* version? */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0)
        goto fail;

    num_samples = read_32bitLE(0x08, streamFile);
    channel_count = read_32bitLE(0x0c,streamFile);
    sample_rate = read_32bitLE(0x10, streamFile);

    loop_flag  = 0;
    start_offset = 0x14;

    if (channel_count > 1)  /* unknown interleave */
        goto fail;
    if (num_samples != ima_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IMA;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_BLITZ_IMA;
    vgmstream->layout_type = layout_none;

    vgmstream->num_samples = num_samples;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
