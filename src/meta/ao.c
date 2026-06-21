#include "meta.h"
#include "../coding/coding.h"

/* ALPHAOGG - from AlphaOgg lib by Ko-Ta [cloudphobia (PC), GEO ~The Sword Millennia~ Kasumi no Tani no Kaibutsu (PC)] */
VGMSTREAM* init_vgmstream_alphaogg(STREAMFILE* sf) {

    /* checks */
    if (!is_id64be(0x00,sf, "ALPHAOGG"))
        return NULL;
    if (!check_extensions(sf,"ao"))
        return NULL;

    ogg_vorbis_meta_info_t ovmi = {0};
    int sample_rate = read_u32le(0xF0, sf); // from Ogg header

    ovmi.meta_type = meta_ALPHAOGG;

    ovmi.loop_start = read_f32le(0x08, sf) * sample_rate;
    ovmi.loop_end = read_f32le(0x0c, sf) * sample_rate; // also num_samples in some versions
    ovmi.loop_end_found = true;
    ovmi.loop_flag = read_u8(0x10, sf) != 0; // count or -1=infinite, u32 in some versions
    /* AlphaOgg defines up to 16 loop points for some reason */

    uint32_t start_offset = 0xc8;
    return init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
}
