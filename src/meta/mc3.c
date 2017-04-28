#include "meta.h"


/* MC3 - from Paradigm games [Spy Hunter, MX Rider, Terminator 3] */
VGMSTREAM * init_vgmstream_mc3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"mc3"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4D504333)   /* "MPC3" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x00011400)   /* version? */
        goto fail;

    start_offset = 0x1c;
    loop_flag = 0;
    channel_count = read_32bitLE(0x08, streamFile);
    if (channel_count > 2) goto fail; /* not seen, decoder must be adapted */
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
    vgmstream->coding_type = coding_MC3;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_MC3;

    vgmstream->sample_rate = read_32bitLE(0x0c, streamFile);
    vgmstream->num_samples = read_32bitLE(0x10, streamFile) * 10; /* sizes in sub-blocks of 10 samples (without headers) */
    vgmstream->interleave_block_size = (read_32bitLE(0x14, streamFile) * 4 * channel_count) + 4;
    if (read_32bitLE(0x18, streamFile) + start_offset != get_streamfile_size(streamFile))
        goto fail;


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
