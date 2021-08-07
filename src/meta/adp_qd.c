#include "meta.h"

/* ADP - from Omikron: The Nomad Soul (PC/DC) */
VGMSTREAM* init_vgmstream_adp_qd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channels, stereo_flag;


    /* checks */
    if (!check_extensions(sf,"adp"))
        goto fail;

    /* no ID, only a basic 0x10 header with filesize and nulls; do some extra checks */
    data_size = read_u32le(0x00,sf) & 0x00FFFFFF; /*24 bit*/
    if (data_size + 0x10 != sf->get_size(sf)
            || read_u32le(0x04,sf) != 0
            || read_u32le(0x08,sf) != 0
            || read_u32le(0x0c,sf) != 0)
        goto fail;

    stereo_flag = read_u8(0x03, sf);
    if (stereo_flag > 1 || stereo_flag < 0) goto fail;
    channels = stereo_flag ? 2 : 1;
    start_offset = 0x10;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_QD_ADP;
    vgmstream->sample_rate = 22050;
    vgmstream->num_samples = data_size * 2 / channels;

    vgmstream->coding_type = coding_QD_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
