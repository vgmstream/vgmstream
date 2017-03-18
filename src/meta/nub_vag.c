#include "meta.h"
#include "../util.h"

/* vag - from Namco's PS3 NUB archives (Ridge Racer 7) */
VGMSTREAM * init_vgmstream_nub_vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "vag")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x76616700) /* "vag\0" */
        goto fail;

    loop_flag = read_32bitBE(0x30,streamFile)==0x3F800000;
    channel_count = 1; /* dual file stereo */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0xBC,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitBE(0x14,streamFile)*28/32*2;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x20,streamFile)*28/32*2;
        vgmstream->loop_end_sample = read_32bitBE(0x24,streamFile)*28/32*2;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NUB_VAG;

    start_offset = 0xC0;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
