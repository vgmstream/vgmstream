#include "meta.h"
#include "../coding/coding.h"

/* PONA - from Policenauts (3DO) */
VGMSTREAM * init_vgmstream_pona_3do(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .pona: fake extension?
     * .sxd: Policenauts Pilot Disc (3DO) */
    if (!check_extensions(streamFile, "pona,sxd"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x13020000)
        goto fail;
    if (read_32bitBE(0x06,streamFile)+0x800 != get_streamfile_size(streamFile))
        goto fail;
    
    loop_flag = (read_32bitBE(0x0A,streamFile) != 0xFFFFFFFF);
    channel_count = 1;
    start_offset = (uint16_t)(read_16bitBE(0x04,streamFile));


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PONA_3DO;
    vgmstream->sample_rate = 22050;
    vgmstream->num_samples = get_streamfile_size(streamFile) - start_offset;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x0A,streamFile);
        vgmstream->loop_end_sample = read_32bitBE(0x06,streamFile);
    }
    vgmstream->coding_type = coding_SDX2;
    vgmstream->layout_type = layout_none;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* PONA - from Policenauts (PSX) */
VGMSTREAM * init_vgmstream_pona_psx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    /* .pona: fake extension? */
    if (!check_extensions(streamFile, "pona"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x00000800)
        goto fail;
    if (read_32bitBE(0x08,streamFile)+0x800 != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (read_32bitBE(0xC,streamFile) != 0xFFFFFFFF);
    channel_count = 1;
    start_offset = read_32bitBE(0x04,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PONA_PSX;
    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitBE(0x0C,streamFile), channel_count);
        vgmstream->loop_end_sample = ps_bytes_to_samples(read_32bitBE(0x08,streamFile), channel_count);
    }
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
