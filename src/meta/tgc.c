#include "meta.h"
#include "../coding/coding.h"

/* Tiger Game.com ADPCM file */
VGMSTREAM* init_vgmstream_tgc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint16_t size;
    off_t start_offset;


    /* checks */
    if (read_u8(0x00, sf) != 0)
        goto fail;

    if (!check_extensions(sf, "4"))
        goto fail;

    size = read_u16be(0x01, sf);
    if (size != get_streamfile_size(sf))
        goto fail;
    start_offset = 0x03;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(1, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 8000;
    vgmstream->num_samples = (size - 0x03) * 2;
    vgmstream->meta_type   = meta_TGC;
    vgmstream->layout_type = layout_none;
    vgmstream->coding_type = coding_TGC;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
