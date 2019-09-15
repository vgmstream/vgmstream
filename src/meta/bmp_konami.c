#include "meta.h"


/* BMP - from Jubeat series (AC) */
VGMSTREAM * init_vgmstream_bmp_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .bin: actual extension
     * .lbin: for plugins */
    if (!check_extensions(streamFile, "bin,lbin"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x424D5000) /* "BMP\0" "*/
        goto fail;

    channel_count = read_8bit(0x10,streamFile); /* assumed */
    if (channel_count != 2) goto fail;
    loop_flag  = 0;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BMP_KONAMI;

    vgmstream->num_samples = read_32bitBE(0x04,streamFile);
    vgmstream->sample_rate = read_32bitBE(0x14, streamFile);

    vgmstream->coding_type = coding_OKI4S;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
