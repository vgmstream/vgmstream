#include "meta.h"
#include "../coding/coding.h"

/* RKV - from Legacy of Kain - Blood Omen 2 (PS2) */
VGMSTREAM * init_vgmstream_ps2_rkv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "rkv"))
        goto fail;

    /* some RKV got info at offset 0x00, some other at 0x0 4 */
    if(read_32bitLE(0x00,streamFile)==0)
        header_offset = 0x04;
    else
        header_offset = 0x00;
    start_offset = 0x800;
    data_size = get_streamfile_size(streamFile) - 0x800;

    loop_flag = (read_32bitLE(header_offset+0x04,streamFile)!=0xFFFFFFFF);
    channel_count = read_32bitLE(header_offset+0x0c,streamFile)+1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(header_offset,streamFile);
    vgmstream->coding_type = coding_PSX;

    /* sometimes sample count is not set on the header */
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(header_offset+0x04,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(header_offset+0x08,streamFile);
    }

    vgmstream->meta_type = meta_PS2_RKV;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
