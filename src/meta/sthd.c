#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* STHD - Dream Factory .stx [Kakuto Chojin (Xbox)] */
VGMSTREAM * init_vgmstream_sthd(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!is_id32be(0x00,sf, "STHD"))
        goto fail;
    if (!check_extensions(sf, "stx"))
        goto fail;
    /* first block has special values */
    if (read_u16le(0x04,sf) != 0x0800 ||
        read_u32le(0x0c,sf) != 0x0001 ||
        read_u32le(0x14,sf) != 0x0000)
        goto fail;

    channel_count = read_s16le(0x06,sf);
    loop_flag = read_s16le(0x18,sf) != -1;
    start_offset = read_u16le(0x04,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STHD;
    vgmstream->sample_rate = read_s32le(0x20, sf); /* repeated ~8 times? */

    vgmstream->coding_type = coding_XBOX_IMA_int;
    vgmstream->layout_type = layout_blocked_sthd;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    /* calc num_samples manually (blocks data varies in size) */
    {
        /* loop values may change to +1 in first actual block, but this works ok enough */
        int loop_start_block = (uint16_t)read_16bitLE(0x1a,sf);
        int loop_end_block   = (uint16_t)read_16bitLE(0x1c,sf);
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
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
