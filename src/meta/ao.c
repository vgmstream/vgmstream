#include "meta.h"
#include "../coding/coding.h"

/* .AO - from AlphaOgg lib [Cloudphobia (PC)] */
VGMSTREAM * init_vgmstream_ao(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;


    /* checks */
    if ( !check_extensions(streamFile,"ao") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x414C5048) /* "ALPH" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x414F4747) /* "AOGG" */
        goto fail;

#ifdef VGM_USE_VORBIS
    {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_AO;
        /* values at 0x08/0x0c/0x10 may be related to looping? */
        start_offset = 0xc8;
        vgmstream = init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, start_offset, &ovmi);
    }
#else
    goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
