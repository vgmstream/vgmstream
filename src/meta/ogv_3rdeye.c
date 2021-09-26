#include "meta.h"
#include "../coding/coding.h"

/* OGV - .ogg container (not related to ogv video) [Bloody Rondo (PC)] */
VGMSTREAM* init_vgmstream_ogv_3rdeye(STREAMFILE* sf) {
    uint32_t subfile_offset, subfile_size;


    /* checks */
    if (!is_id32be(0x00,sf, "OGV\0"))
        goto fail;
    if (!check_extensions(sf,"ogv"))
        goto fail;

    /* 0x04: PCM size */
    subfile_size = read_u32le(0x08, sf);
    /* 0x0c: "fmt" + RIFF fmt + "data" (w/ PCM size too) */
    subfile_offset = 0x2c;

    /* no loops (files bgm does full loops but sfx doesn't) */

    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_OGV_3RDEYE;
        ovmi.stream_size = subfile_size;

        return init_vgmstream_ogg_vorbis_config(sf, subfile_offset, &ovmi);
    }

fail:
    return NULL;
}
