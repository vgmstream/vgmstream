#include "meta.h"

/* ADP - from Balls of Steel */
VGMSTREAM* init_vgmstream_adp_bos(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0;
    int channels;

    /* checks */
    if (!check_extensions(sf,"adp"))
        goto fail;

    if (!is_id32be(0x00,sf, "ADP!"))
        goto fail;

    loop_flag = (-1 != read_s32le(0x08,sf));
    channels = 1;
    start_offset = 0x18;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x0C,sf);
    vgmstream->num_samples = read_s32le(0x04,sf);
    vgmstream->loop_start_sample = read_s32le(0x08,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_BOS_ADP;

    // 0x10, 0x12 - both initial history?
    //vgmstream->ch[0].adpcm_history1_32 = read_16bitLE(0x10,sf);
    // 0x14 - initial step index?
    //vgmstream->ch[0].adpcm_step_index = read_32bitLE(0x14,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
