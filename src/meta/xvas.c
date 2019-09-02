#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* XVAS - found in TMNT 2 & TMNT 3 (Xbox) */
VGMSTREAM * init_vgmstream_xvas(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile,"xvas"))
        goto fail;
    if (read_32bitLE(0x00,streamFile) != 0x69 && /* codec */
        read_32bitLE(0x08,streamFile) != 0x48)   /* block size (probably 0x24 for mono) */
        goto fail;

    start_offset = 0x800;
    channel_count = read_32bitLE(0x04,streamFile); /* always stereo files */
    loop_flag = (read_32bitLE(0x14,streamFile) == read_32bitLE(0x24,streamFile));
    data_size = read_32bitLE(0x24,streamFile);
    data_size -= (data_size / 0x20000) * 0x20; /* blocks of 0x20000 with padding */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XVAS;
    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    if(loop_flag) {
        size_t loop_size = read_32bitLE(0x10,streamFile);
        loop_size -= (loop_size / 0x20000) * 0x20;
        vgmstream->loop_start_sample = xbox_ima_bytes_to_samples(loop_size, vgmstream->channels);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_blocked_xvas;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
