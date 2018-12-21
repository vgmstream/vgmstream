#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* .VS - from Melbourne House games [Men in Black II (PS2), Grand Prix Challenge (PS2) */
VGMSTREAM * init_vgmstream_vs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "vs"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0xC8000000)
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    start_offset = 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS;
    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
        block_update(start_offset, vgmstream);

    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
