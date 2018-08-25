#include "meta.h"
#include "../coding/coding.h"
#include "kma9_streamfile.h"


/* KMA9 - Koei Tecmo's interleaved ATRAC9 [Nobunaga no Yabou - Souzou (Vita)] */
VGMSTREAM * init_vgmstream_kma9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE* temp_streamFile = NULL;
    off_t start_offset;
    size_t stream_size, interleave;
    int loop_flag, channel_count;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;


    /* checks */
    if ( !check_extensions(streamFile,"km9") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4B4D4139) /* "KMA9" */
        goto fail;

    start_offset = read_32bitLE(0x04,streamFile);
    channel_count = read_16bitLE(0x32,streamFile);
    loop_flag = (read_32bitLE(0x28,streamFile) != 0);

    total_subsongs = read_32bitLE(0x08,streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* 0x0c: unknown */
    interleave = read_32bitLE(0x10,streamFile); /* 1 superframe */
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

        cfg.channels = vgmstream->channels;
        cfg.encoder_delay = read_32bitLE(0x20,streamFile);
        cfg.config_data = read_32bitBE(0x5c,streamFile);

        vgmstream->codec_data = init_atrac9(&cfg);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_ATRAC9;
        vgmstream->layout_type = layout_none;

        if (loop_flag) { /* seems correct but must double check */
            vgmstream->loop_start_sample -= cfg.encoder_delay;
            //vgmstream->loop_end_sample -= cfg.encoder_delay;
        }

        /* KMA9 interleaves one ATRAC9 frame per subsong */
        temp_streamFile = setup_kma9_streamfile(streamFile, start_offset, stream_size, interleave, (target_subsong-1), total_subsongs);
        if (!temp_streamFile) goto fail;
        start_offset = 0;
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, temp_streamFile, start_offset) )
        goto fail;
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
