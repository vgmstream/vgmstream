#include "meta.h"
#include "../coding/coding.h"

/* APC - from Cryo games [MegaRace 3 (PC)] */
VGMSTREAM* init_vgmstream_apc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "CRYO"))
        goto fail;
    if (!is_id32be(0x04,sf, "_APC"))
        goto fail;
  //if (!is_id32be(0x04,sf, "1.20"))
  //    goto fail;

    if (!check_extensions(sf,"apc"))
        goto fail;

    sample_rate = read_s32le(0x10,sf);
    /* 0x14/18: L/R hist sample? */
    channels = read_s32le(0x1c,sf) == 0 ? 1 : 2;
    loop_flag = 0;
    start_offset = 0x20;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_APC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ima_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
