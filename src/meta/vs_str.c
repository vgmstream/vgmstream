#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/layout_utils.h"

/* .vs/STRx - from The Bouncer (PS2) */
VGMSTREAM* init_vgmstream_vs_str(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    off_t start_offset;


    /* checks */
    if (!(is_id32be(0x000,sf, "STRL") && is_id32be(0x800,sf, "STRR")) &&  !is_id32be(0x00,sf, "STRM"))
        return NULL;
    /* .vs: real extension (from .nam container)
     * .str: fake, partial header id */
    if (!check_extensions(sf, "vs,str"))
        return NULL;


    loop_flag = 0;
    channels = (is_id32be(0x00,sf, "STRM")) ? 1 : 2; // "STRM"=mono (voices)
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VS_STR;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_vs_str;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
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
