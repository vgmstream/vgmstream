#include "meta.h"
#include "../coding/coding.h"

/* KMA9 - Koei Tecmo's custom ATRAC9 [Nobunaga no Yabou - Souzou (Vita)] */
VGMSTREAM * init_vgmstream_kma9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t stream_size;
    int loop_flag, channel_count;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;


    /* check extension */
    if ( !check_extensions(streamFile,"km9") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4B4D4139) /* "KMA9" */
        goto fail;

    start_offset = read_32bitLE(0x04,streamFile);
    channel_count = read_16bitLE(0x32,streamFile);
    loop_flag = (read_32bitLE(0x28,streamFile) != 0);

    total_subsongs = read_32bitLE(0x08,streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* 0x0c: unknown */
    stream_size = read_32bitLE(0x14,streamFile); /* per subsong */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x34,streamFile);
    vgmstream->num_samples = read_32bitLE(0x18,streamFile); /* without skip_samples? */
    vgmstream->loop_start_sample = read_32bitLE(0x24,streamFile); /* with skip_samples? */
    vgmstream->loop_end_sample = vgmstream->num_samples; /* 0x28 looks like end samples but isn't, no idea */
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_KMA9;

#ifdef VGM_USE_ATRAC9
    {
        atrac9_config cfg = {0};

        cfg.type = ATRAC9_KMA9;
        cfg.channels = vgmstream->channels;
        cfg.config_data = read_32bitBE(0x5c,streamFile);
        cfg.encoder_delay = read_32bitLE(0x20,streamFile);

        cfg.interleave_skip = read_32bitLE(0x10,streamFile); /* 1 superframe */
        cfg.subsong_skip = total_subsongs;
        start_offset += (target_subsong-1) * cfg.interleave_skip * (cfg.subsong_skip-1);

        vgmstream->codec_data = init_atrac9(&cfg);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_ATRAC9;
        vgmstream->layout_type = layout_none;

        if (loop_flag) { /* seems correct but must double check */
            vgmstream->loop_start_sample -= cfg.encoder_delay;
            //vgmstream->loop_end_sample -= cfg.encoder_delay;
        }
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
