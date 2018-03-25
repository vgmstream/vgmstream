#include "meta.h"
#include "../coding/coding.h"

/* .SMV - from Cho Aniki Zero (PSP) */
VGMSTREAM * init_vgmstream_smv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t channel_size, loop_start;


    /* check extension */
    if (!check_extensions(streamFile, "smv"))
        goto fail;

    channel_size = read_32bitLE(0x00,streamFile);
    /* 0x08: number of full interleave blocks */
    channel_count = read_16bitLE(0x0a,streamFile);
    loop_start = read_32bitLE(0x18,streamFile);
    loop_flag = (loop_start != -1);
    start_offset = 0x800;

    if (channel_size * channel_count + start_offset != get_streamfile_size(streamFile))
        goto fail;

    channel_size -= 0x10; /* last value has SPU end frame without flag 0x7 as it should */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10, streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size*channel_count, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start*channel_count, channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_SMV;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x04, streamFile);
    vgmstream->interleave_last_block_size = read_32bitLE(0x0c, streamFile);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
