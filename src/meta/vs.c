#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/layout_utils.h"


/* .VS - from Melbourne House games [Men in Black II (PS2), Grand Prix Challenge (PS2) */
VGMSTREAM* init_vgmstream_vs_mh(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (read_u32be(0x00,sf) != 0xC8000000)
        return NULL;
    if (!check_extensions(sf, "vs"))
        return NULL;

    /* extra checks since format is too simple */
    int sample_rate = read_s32le(0x04,sf);
    if (sample_rate != 48000 && sample_rate != 44100)
        return NULL;
    if (read_u32le(0x08,sf) != 0x1000)
        return NULL;
    if (!ps_check_format(sf, 0x0c, 0x1000))
        return NULL;


    loop_flag = 0;
    channels = 2;
    start_offset = 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS_MH;
    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs_mh;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    {
        blocked_counter_t cfg = {0};
        cfg.offset = start_offset;

        blocked_count_samples(vgmstream, sf, &cfg);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
