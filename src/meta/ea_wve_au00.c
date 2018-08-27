#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* EA WVE (VLC0/au00) - from Electronic Arts PS movies [Future Cop - L.A.P.D. (PS), Supercross 2000 (PS)] */
VGMSTREAM * init_vgmstream_ea_wve_au00(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .wve: common, .fsv: Future Cop LAPD (PS1) */
    if (!check_extensions(streamFile, "wve,fsv"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x564C4330) /* "VLC0" */
        goto fail;

    start_offset = read_32bitBE(0x04,streamFile);
    if (read_32bitBE(start_offset, streamFile) != 0x61753030 &&  /* "au00" */
        read_32bitBE(start_offset, streamFile) != 0x61753031)    /* "au01" (last block, but could be first) */
        goto fail;
    loop_flag  = 0;
    channel_count = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_EA_WVE_AU00;
    vgmstream->sample_rate = 22050;

    /* You'd think they'd use coding_EA_XA_int but instead it's PS-ADPCM without flags and 0x0f frame size
     * (equivalent to configurable PS-ADPCM), surely to shoehorn EA-XA sizes into the PS1 hardware decoder */
    vgmstream->coding_type = coding_PSX_cfg;
    vgmstream->interleave_block_size = 0x0f;
    vgmstream->layout_type = layout_blocked_ea_wve_au00;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples manually */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_cfg_bytes_to_samples(vgmstream->current_block_size, vgmstream->interleave_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
