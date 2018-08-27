#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* EA WVE (Ad10) - from Electronic Arts PS movies [Wing Commander 3/4 (PS)] */
VGMSTREAM * init_vgmstream_ea_wve_ad10(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "wve"))
        goto fail;

    start_offset = 0x00;
    if (read_32bitBE(start_offset, streamFile) != 0x41643130 &&  /* "Ad10" */
        read_32bitBE(start_offset, streamFile) != 0x41643131)    /* "Ad11" (last block, but could be first) */
        goto fail;
    loop_flag  = 0;
    /* no header = no channels, but seems if the first PS-ADPCM header is 00 then it's mono, somehow
     * (ex. Wing Commander 3 intro / Wing Commander 4 = stereo, rest of Wing Commander 3 = mono) */
    channel_count = read_8bit(start_offset+0x08,streamFile) != 0 ? 2 : 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 22050;
    vgmstream->meta_type = meta_EA_WVE_AD10;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_ea_wve_ad10;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples manually */
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
