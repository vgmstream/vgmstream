#include "meta.h"
#include "../coding/coding.h"

/* .AO - from AlphaOgg lib [Cloudphobia (PC), GEO ~The Sword Millennia~ Kasumi no Tani no Kaibutsu (PC)] */
VGMSTREAM* init_vgmstream_ao(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;


    /* checks */
    if (!is_id64be(0x00,sf, "ALPHAOGG"))
        goto fail;
    if (!check_extensions(sf,"ao"))
        goto fail;

    {
        ogg_vorbis_meta_info_t ovmi = {0};
        int sample_rate = read_u32le(0xF0, sf); /* Ogg header */

        ovmi.meta_type = meta_AO;

        ovmi.loop_start = read_f32le(0x08, sf) * sample_rate;
        ovmi.loop_end = read_f32le(0x0c, sf) * sample_rate; /* also num_samples in some versions */
        ovmi.loop_end_found = 1;
        ovmi.loop_flag = read_u8(0x10, sf) != 0; /* count or -1=infinite, u32 in some versions */
        /* AlphaOgg defines up to 16 loop points for some reason */

        start_offset = 0xc8;
        vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
    }

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
