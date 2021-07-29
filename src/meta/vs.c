#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* .VS - from Melbourne House games [Men in Black II (PS2), Grand Prix Challenge (PS2) */
VGMSTREAM* init_vgmstream_vs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "vs"))
        goto fail;
    if (read_u32be(0x00,sf) != 0xC8000000)
        goto fail;

    loop_flag = 0;
    channels = 2;
    start_offset = 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS;
    vgmstream->sample_rate = read_s32le(0x04,sf);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    /* calc num_samples */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);

    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
