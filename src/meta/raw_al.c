#include "meta.h"
#include "../coding/coding.h"

/* AL/AL2 - headerless a-law, from Illwinter Game Design games */
VGMSTREAM * init_vgmstream_raw_al(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;

    /* checks */
    /* .al: Dominions 3 - The Awakening (PC)
     * .al2: Conquest of Elysium 3 (PC) */
    if ( !check_extensions(streamFile,"al,al2"))
        goto fail;

    channel_count = check_extensions(streamFile,"al") ? 1 : 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate =  22050;
    vgmstream->coding_type = coding_ALAW;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->meta_type = meta_RAW_AL;
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile), channel_count, 8);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    start_offset = 0;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
