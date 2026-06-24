#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util.h"
#include "../util/layout_utils.h"


/* .IAB - from Runtime(?) games [Ueki no Housoku: Taosu ze Robert Juudan!! (PS2), RPG Maker 3 (PS2)] */
VGMSTREAM* init_vgmstream_iab(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (read_u32be(0x00,sf) != 0x10000000)
        return NULL;
    if (!check_extensions(sf,"iab"))
        return NULL;
    if (read_u32le(0x1c,sf) != get_streamfile_size(sf))
        return NULL;

    loop_flag = 0;
    channels = 2;
    start_offset = 0x40;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IAB;
    vgmstream->sample_rate = read_s32le(0x04,sf);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_ps2_iab;
    //vgmstream->interleave_block_size = read_32bitLE(0x0C, sf); /* unneeded */

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
