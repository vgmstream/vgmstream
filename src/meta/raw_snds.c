#include "meta.h"


/* .snds - from Heavy Iron's The Incredibles (PC) */
VGMSTREAM * init_vgmstream_raw_snds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t file_size;
    int i;


    /* checks */
    if (!check_extensions(streamFile, "snds"))
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    start_offset = 0;
    file_size = get_streamfile_size(streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAW_SNDS;
    vgmstream->sample_rate = 48000;

    /* file seems to be mistakenly 1/8 too long, check for 32 0 bytes where the padding should start */
    vgmstream->num_samples = file_size*8/9;
    for (i = 0; i < 8; i++) {
        if (read_32bitBE(vgmstream->num_samples+i*4,streamFile) != 0) {
            vgmstream->num_samples = file_size; /* no padding? just play the whole file */
            break;
        }
    }

    vgmstream->coding_type = coding_SNDS_IMA;
    vgmstream->layout_type = layout_none;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

