#include "meta.h"
#include "../coding/coding.h"

/* RAW - RAW format is native 44khz PCM file */
VGMSTREAM * init_vgmstream_raw_pcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "raw"))
        goto fail;

    channel_count = 2;
    loop_flag  = 0;
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAW_PCM;
    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile), channel_count, 16);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
