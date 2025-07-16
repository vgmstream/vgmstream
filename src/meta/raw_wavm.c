#include "meta.h"
#include "../coding/coding.h"

/* WAVM - headerless format which can be found on XBOX */
VGMSTREAM * init_vgmstream_raw_wavm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0;
    int loop_flag, channel_count;

    /* check extension */
    if (!check_extensions(streamFile,"wavm"))
        goto fail;

    start_offset = 0;
    loop_flag = 0;
    channel_count = 2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAW_WAVM;
    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = xbox_ima_bytes_to_samples(get_streamfile_size(streamFile), vgmstream->channels);

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
