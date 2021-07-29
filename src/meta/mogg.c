/*
2017-12-10: Preliminary MOGG Support. As long as the stream is unencrypted, this should be fine.
            This will also work on unconventional 5 channel Vorbis streams but some sound cards might not like it.
            TODO (Eventually): Add decryption for encrypted MOGG types (Rock Band, etc.)

            -bxaimc
*/

#include "meta.h"
#include "../coding/coding.h"

/* MOGG - Harmonix Music Systems (Guitar Hero)[Unencrypted Type] */
VGMSTREAM* init_vgmstream_mogg(STREAMFILE *sf) {
#ifdef VGM_USE_VORBIS
    off_t start_offset;

    /* checks */
    if (!check_extensions(sf, "mogg"))
        goto fail;

    {
        ogg_vorbis_meta_info_t ovmi = {0};
        VGMSTREAM * result = NULL;

        ovmi.meta_type = meta_MOGG;

        start_offset = read_32bitLE(0x04, sf);
        result = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);

        if (result != NULL) {
            return result;
        }
    }

fail:
    /* clean up anything we may have opened */
#endif
    return NULL;
}
