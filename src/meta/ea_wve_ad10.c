#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"


/* EA WVE (Ad10) - from early Electronic Arts movies [Wing Commander 3/4 (PS1), Madden NHL 97 (PC)-w95] */
VGMSTREAM* init_vgmstream_ea_wve_ad10(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00, sf, "AABB") &&  /* video block */
        !is_id32be(0x00, sf, "Ad10") &&  /* audio block */
        !is_id32be(0x00, sf, "Ad11"))    /* last audio block, but could be first */
        return NULL;

    /* .wve: common
     * .mov: Madden NHL 97 (also uses .wve) */
    if (!check_extensions(sf, "wve,mov"))
        return NULL;

    bool big_endian = guess_endian32(0x04, sf);

    start_offset = 0x00;
    if (is_id32be(0x00, sf, "AABB")){
        start_offset += big_endian ?  read_u32be(0x04, sf) : read_u32le(0x04, sf);
    }

    bool is_ps1;
    if (ps_check_format(sf, start_offset + 0x08, 0x40)) {
        /* no header = no channels, but seems if the first PS-ADPCM header is 00 then it's mono, somehow
        * (ex. Wing Commander 3 intro / Wing Commander 4 = stereo, rest of Wing Commander 3 = mono) */
        channels = read_u8(start_offset + 0x08,sf) != 0 ? 2 : 1;
        is_ps1 = true;
    }
    else {
        channels = 1;
        is_ps1 = false;
    }

    loop_flag = false;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 22050;
    vgmstream->meta_type = meta_EA_WVE_AD10;
    vgmstream->layout_type = layout_blocked_ea_wve_ad10;
    vgmstream->codec_config = big_endian;
    if (is_ps1)
        vgmstream->coding_type = coding_PSX;
    else
        vgmstream->coding_type = coding_PCM8_U_int;

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
