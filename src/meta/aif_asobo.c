#include "meta.h"
#include "../coding/coding.h"

/* .AIF - from Asobo Studio games [Ratatouille (PC), WALL-E (PC), Up (PC)] */
VGMSTREAM* init_vgmstream_aif_asobo(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (read_u16le(0x00,sf) != 0x69) /* fmt chunk with Xbox codec */
        goto fail;
    /* aif: standard, .laif: for plugins */
    if ( !check_extensions(sf,"aif,laif") )
        goto fail;

    channels = read_u16le(0x02,sf); /* assumed, only stereo is known */
    if (channels != 2) goto fail;

    sample_rate = read_u32le(0x04,sf);
    /* 0x08: bitrate */
    if (read_u16le(0x0c,sf) != 0x24 * channels) /* Xbox block */
        goto fail;
    if (read_u16le(0x0e,sf) != 0x04) /* Xbox bps */
        goto fail;
    loop_flag = 0;

    start_offset = 0x14;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AIF_ASOBO;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
