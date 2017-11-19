#include "meta.h"

/* DVI - from Konami KCE Yokohama DC games (Pop'n Music series) */
VGMSTREAM * init_vgmstream_dc_kcey(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension (.pcm: original, .kcey: renamed to header id) */
    if ( !check_extensions(streamFile,"pcm,kcey") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4B434559) /* "KCEY" (also "COMP") */
        goto fail;

    start_offset = read_32bitBE(0x10,streamFile);
    loop_flag = (read_32bitBE(0x14,streamFile) != 0xFFFFFFFF);
    channel_count = read_32bitBE(0x08,streamFile);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 37800;
    vgmstream->num_samples = read_32bitBE(0x0C,streamFile);
    vgmstream->loop_start_sample = read_32bitBE(0x14,streamFile);
    vgmstream->loop_end_sample = read_32bitBE(0x0C,streamFile);

    vgmstream->coding_type = coding_DVI_IMA; /* stereo/mono, high nibble first */
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DC_KCEY;


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
