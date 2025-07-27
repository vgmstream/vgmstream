#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* EA WVE (VLC0/au00) - from Electronic Arts PS movies [Future Cop - L.A.P.D. (PS), Supercross 2000 (PS)] */
VGMSTREAM* init_vgmstream_ea_wve_au00(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "VLC0"))
        return NULL;

    /* .wve: common, .fsv: Future Cop LAPD (PS1) */
    if (!check_extensions(sf, "wve,fsv"))
        return NULL;

    start_offset = read_32bitBE(0x04,sf);
    if (!is_id32be(start_offset, sf, "au00") &&
        !is_id32be(start_offset, sf, "au01")) // last block, but could be first)
        return NULL;
    loop_flag  = 0;
    channels = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_EA_WVE_AU00;
    vgmstream->sample_rate = 22050;

    /* You'd think they'd use coding_EA_XA_int but instead it's PS-ADPCM without flags and 0x0f frame size
     * (equivalent to configurable PS-ADPCM), surely to shoehorn EA-XA sizes into the PS1 hardware decoder */
    vgmstream->coding_type = coding_PSX_cfg;
    vgmstream->interleave_block_size = 0x0f;
    vgmstream->layout_type = layout_blocked_ea_wve_au00;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    /* calc num_samples manually */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += ps_cfg_bytes_to_samples(vgmstream->current_block_size, vgmstream->interleave_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
