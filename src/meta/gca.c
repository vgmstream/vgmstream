#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* GCA - Terminal Reality games [Metal Slug Anthology (Wii), BlowOut (GC)] */
VGMSTREAM* init_vgmstream_gca(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "GCA1"))
        goto fail;
    if (!check_extensions(sf, "gca"))
        goto fail;

    start_offset = 0x40;
    loop_flag = 0;
    channels = 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GCA;
    vgmstream->sample_rate = read_32bitBE(0x2A,sf);
    vgmstream->num_samples = dsp_nibbles_to_samples(read_32bitBE(0x26,sf));//read_32bitBE(0x26,streamFile)*7/8;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    dsp_read_coefs_be(vgmstream, sf, 0x04, 0x00);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
