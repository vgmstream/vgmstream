#include "meta.h"


/* BMP - from Konami arcade games [drummania (AC), GITADORA (AC), Jubeat (AC)] */
VGMSTREAM* init_vgmstream_bmp_konami(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    /* .bin: actual extension
     * .lbin: for plugins */
    if (!check_extensions(sf, "bin,lbin"))
        goto fail;

    if (!is_id32be(0x00,sf, "BMP\0"))
        goto fail;

    channels = read_u8(0x10,sf); /* assumed */
    if (channels != 2) goto fail;
    loop_flag  = 0;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BMP_KONAMI;

    vgmstream->num_samples = read_u32be(0x04,sf);
    vgmstream->sample_rate = read_u32be(0x14, sf);

    vgmstream->coding_type = coding_OKI4S;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
