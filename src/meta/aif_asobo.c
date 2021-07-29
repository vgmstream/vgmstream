#include "meta.h"
#include "../coding/coding.h"

/* .AIF - from Asobo Studio games [Ratatouille (PC), WALL-E (PC), Up (PC)] */
VGMSTREAM* init_vgmstream_aif_asobo(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    /* aif: standard, .laif/aiffl: for plugins */
    if ( !check_extensions(sf,"aif,laif,aiffl") )
        goto fail;
    if ((uint16_t)read_16bitLE(0x00,sf) != 0x69) /* Xbox codec */
        goto fail;

    channel_count = read_16bitLE(0x02,sf); /* assumed, only stereo is known */
    if (channel_count != 2) goto fail;

    /* 0x08: ? */
    if ((uint16_t)read_16bitLE(0x0c,sf) != 0x24*channel_count) /* Xbox block */
        goto fail;
    if ((uint16_t)read_16bitLE(0x0e,sf) != 0x04) /* Xbox bps */
        goto fail;
    loop_flag = 0;

    start_offset = 0x14;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AIF_ASOBO;
    vgmstream->sample_rate = read_32bitLE(0x04,sf);
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
