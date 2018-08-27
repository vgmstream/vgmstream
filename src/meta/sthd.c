#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* STHD - Dream Factory .stx [Kakuto Chojin (Xbox)] */
VGMSTREAM * init_vgmstream_sthd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "stx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53544844) /* "STHD" */
        goto fail;
    /* first block has special values */
    if (read_32bitLE(0x04,streamFile) != 0x0800 &&
        read_32bitLE(0x0c,streamFile) != 0x0001 &&
        read_32bitLE(0x14,streamFile) != 0x0000)
        goto fail;

    channel_count = read_16bitLE(0x06,streamFile);
    loop_flag = read_16bitLE(0x18,streamFile) != -1;
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STHD;
    vgmstream->sample_rate = read_32bitLE(0x20, streamFile); /* repeated ~8 times? */

    vgmstream->coding_type = coding_XBOX_IMA_int;
    vgmstream->layout_type = layout_blocked_sthd;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples manually (blocks data varies in size) */
    {
        /* loop values may change to +1 in first actual block, but this works ok enough */
        int loop_start_block = (uint16_t)read_16bitLE(0x1a,streamFile);
        int loop_end_block   = (uint16_t)read_16bitLE(0x1c,streamFile);
        int block_count = 1; /* header block = 0 */

        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            if (block_count == loop_start_block)
                vgmstream->loop_start_sample = vgmstream->num_samples;
            if (block_count == loop_end_block)
                vgmstream->loop_end_sample = vgmstream->num_samples;

            vgmstream->num_samples += xbox_ima_bytes_to_samples(vgmstream->current_block_size, 1);
            block_count++;
        }
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
