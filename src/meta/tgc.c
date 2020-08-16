#include "meta.h"
#include "../coding/coding.h"

/* Tiger Game.com ADPCM file */
VGMSTREAM * init_vgmstream_tgc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;

    /* checks */
    if (!check_extensions(streamFile, "4"))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(1, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 8000;
    vgmstream->num_samples = ((uint16_t)read_16bitBE(1, streamFile) - 3) * 2;
    vgmstream->meta_type   = meta_TGC;
    vgmstream->layout_type = layout_none;
    vgmstream->coding_type = coding_TGC;
    
    vgmstream->ch[0].adpcm_history1_32 = 0;
    vgmstream->ch[0].adpcm_step_index  = 0;

    if (!vgmstream_open_stream(vgmstream, streamFile, 3))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
