#include "meta.h"
#include "../coding/coding.h"

/* UTK - from Maxis games [The Sims Online (PC), SimCity 4 (PC)] */
VGMSTREAM * init_vgmstream_utk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "utk"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x55544D30) /* "UTM0" */
        goto fail;
    if (read_32bitLE(0x08,streamFile) != 0x14) /* header size? */
        goto fail;
    if (read_16bitLE(0x0c,streamFile) != 0x01) /* codec */
        goto fail;
    /* 0x14: avg bitrate, 0x18: PCM block size, 0x1a: PCM bits, 0x1c: extra size? */

    channel_count = read_16bitLE(0x0e,streamFile);
    if (channel_count > 1) goto fail; /* unknown */
    loop_flag  = 0;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UTK;
    vgmstream->sample_rate = read_32bitLE(0x10, streamFile);
    vgmstream->num_samples = read_32bitLE(0x04, streamFile) / 2; /* PCM size */
    vgmstream->coding_type = coding_EA_MT;
    vgmstream->layout_type = layout_none;
    vgmstream->codec_data = init_ea_mt(vgmstream->channels, 0);
    if (!vgmstream->codec_data) goto fail;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
