#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util.h"

/* .IAB - from Runtime(?) games [Ueki no Housoku - Taosu ze Robert Juudan!! (PS2), RPG Maker 3 (PS2)] */
VGMSTREAM * init_vgmstream_ps2_iab(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
	

	/* checks */
    if (!check_extensions(streamFile,"iab"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x10000000)
        goto fail;
    if (read_32bitLE(0x1C,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    start_offset = 0x40;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    vgmstream->meta_type = meta_PS2_IAB;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_ps2_iab;
    //vgmstream->interleave_block_size = read_32bitLE(0x0C, streamFile); /* unneeded */
    
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset, vgmstream);
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
