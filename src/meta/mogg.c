#include "meta.h"
#include "../coding/coding.h"

/* MOGG - Harmonix Music Systems's Ogg (unencrypted type) [Guitar Hero II (X360)] */
VGMSTREAM* init_vgmstream_mogg(STREAMFILE* sf) {
    off_t start_offset;


    /* checks */
    if (read_u32le(0x00, sf) != 0x0A) /* type? */
        goto fail;

    if (!check_extensions(sf, "mogg"))
        goto fail;

    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_MOGG;

        start_offset = read_u32le(0x04, sf);
        return init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
    }

fail:
    return NULL;
}
