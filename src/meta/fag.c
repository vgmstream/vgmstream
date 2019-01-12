#include "meta.h"
#include "../coding/coding.h"

/* .FAG - from Jackie Chan: Stuntmaster (PS1) */
VGMSTREAM * init_vgmstream_fag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t stream_size;
    int loop_flag, channel_count;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile,"fag"))
        goto fail;

    total_subsongs = read_32bitLE(0x00,streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    if (total_subsongs > 2)
        goto fail;
    loop_flag = 0;
    channel_count = 2;

    start_offset = read_32bitLE(0x04 + 0x04*(target_subsong-1),streamFile);
    stream_size  = read_32bitLE(0x04 + 0x04*total_subsongs + 0x04*(target_subsong-1),streamFile) - start_offset; /* end offset */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FAG;
    vgmstream->sample_rate = 22050;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8000;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
