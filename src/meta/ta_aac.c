#include "meta.h"
#include "../coding/coding.h"

/* AAC - tri-Ace (Aska engine) Audio Container */

/* Xbox 360 Variants (Star Ocean 4, End of Eternity, Infinite Undiscovery) */
VGMSTREAM * init_vgmstream_ta_aac_x360(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t sampleRate, numSamples, startSample, dataSize, blockSize, blockCount; // A mess

    /* check extension, case insensitive */
    /* .aac: expected, .laac/ace: for players to avoid hijacking MP4/AAC */
    if ( !check_extensions(streamFile,"aac,laac,ace"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x41414320)   /* "AAC " */
        goto fail;

    /* Ok, let's check what's behind door number 1 */
    if (read_32bitBE(0x1000, streamFile) == 0x41534320) /* "ASC " */
    {
        loop_flag = read_32bitBE(0x1118, streamFile);

        /*Funky Channel Count Checking */
        if (read_32bitBE(0x1184, streamFile) == 0x7374726D)
            channel_count = 6;
        else if (read_32bitBE(0x1154, streamFile) == 0x7374726D)
            channel_count = 4;
        else
            channel_count = read_8bit(0x1134, streamFile);

        sampleRate = read_32bitBE(0x10F4, streamFile);
        numSamples = read_32bitBE(0x10FC, streamFile);
        startSample = read_32bitBE(0x10F8, streamFile);
        dataSize = read_32bitBE(0x10F0, streamFile);
        blockSize = read_32bitBE(0x1100, streamFile);
        blockCount = read_32bitBE(0x110C, streamFile);
    }
    else if (read_32bitBE(0x1000, streamFile) == 0x57415645) /* "WAVE" */
    {
        loop_flag = read_32bitBE(0x1048, streamFile);

        /*Funky Channel Count Checking */
        if (read_32bitBE(0x10B0, streamFile) == 0x7374726D)
            channel_count = 6;
        else if (read_32bitBE(0x1080, streamFile) == 0x7374726D)
            channel_count = 4;
        else
            channel_count = read_8bit(0x1060, streamFile);

        sampleRate = read_32bitBE(0x1024, streamFile);
        numSamples = read_32bitBE(0x102C, streamFile);
        startSample = read_32bitBE(0x1028, streamFile);
        dataSize = read_32bitBE(0x1020, streamFile);
        blockSize = read_32bitBE(0x1030, streamFile);
        blockCount = read_32bitBE(0x103C, streamFile);
    }
    else if (read_32bitBE(0x1000, streamFile) == 0x00000000) /* some like to be special */
    {
        loop_flag = read_32bitBE(0x6048, streamFile);

        /*Funky Channel Count Checking */
        if (read_32bitBE(0x60B0, streamFile) == 0x7374726D)
            channel_count = 6;
        else if (read_32bitBE(0x6080, streamFile) == 0x7374726D)
            channel_count = 4;
        else
            channel_count = read_8bit(0x6060, streamFile);

        sampleRate = read_32bitBE(0x6024, streamFile);
        numSamples = read_32bitBE(0x602C, streamFile);
        startSample = read_32bitBE(0x6028, streamFile);
        dataSize = read_32bitBE(0x6020, streamFile);
        blockSize = read_32bitBE(0x6030, streamFile);
        blockCount = read_32bitBE(0x603C, streamFile);
    }
    else
        goto fail; //cuz I don't know if there are other variants

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    if (read_32bitBE(0x1000, streamFile) == 0x00000000)
        start_offset = 0x7000;
    else
        start_offset = 0x2000;

    vgmstream->sample_rate = sampleRate;
    vgmstream->channels = channel_count;
    vgmstream->num_samples = numSamples;
    if (loop_flag) {
        vgmstream->loop_start_sample = startSample;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }
    vgmstream->meta_type = meta_TA_AAC_X360;

#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data *ffmpeg_data = NULL;
        uint8_t buf[100];
        size_t bytes, datasize, block_size, block_count;

        block_count = blockCount;
        block_size = blockSize;
        datasize = dataSize;

        bytes = ffmpeg_make_riff_xma2(buf,100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
        ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
        if ( !ffmpeg_data ) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, streamFile, start_offset, datasize, 0, 1,1);
        if (loop_flag) { /* reapply adjusted samples */
            vgmstream->loop_end_sample = vgmstream->num_samples;
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

/* PlayStation 3 Variants (Star Ocean International, Resonance of Fate) */
VGMSTREAM * init_vgmstream_ta_aac_ps3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    uint32_t data_size, loop_start, loop_end, codec_id, asc_chunk;

    /* check extension, case insensitive */
    /* .aac: expected, .laac/ace: for players to avoid hijacking MP4/AAC */
    if (!check_extensions(streamFile, "aac,laac,ace"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x41414320)   /* "AAC " */
        goto fail;

    /* Find the ASC chunk, That's where the goodies are */
    asc_chunk = read_32bitBE(0x40, streamFile);
    if (read_32bitBE(asc_chunk, streamFile) != 0x41534320)   /* "ASC " */
        goto fail;

    if (read_32bitBE(asc_chunk+0x104, streamFile) != 0xFFFFFFFF)
        loop_flag = 1;
    else
        loop_flag = 0;

    channel_count = read_32bitBE(asc_chunk + 0xF4, streamFile);
    codec_id = read_32bitBE(asc_chunk + 0xF0, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* ASC header */
    start_offset = asc_chunk + 0x110;
    vgmstream->sample_rate = read_32bitBE(asc_chunk + 0xFC, streamFile);
    vgmstream->channels = channel_count;
    vgmstream->meta_type = meta_TA_AAC_PS3;
    data_size = read_32bitBE(asc_chunk + 0xF8, streamFile);
    loop_start = read_32bitBE(asc_chunk + 0x104, streamFile);
    loop_end = read_32bitBE(asc_chunk + 0x108, streamFile);

#ifdef VGM_USE_FFMPEG
    {
        int block_align, encoder_delay;

        block_align = (codec_id == 4 ? 0x60 : (codec_id == 5 ? 0x98 : 0xC0)) * vgmstream->channels;
        encoder_delay = 1024 + 69; /* approximate, gets good loops */
        vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_align) - encoder_delay;

        vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamFile, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
        vgmstream->loop_start_sample = atrac3_bytes_to_samples(loop_start, block_align); // - encoder_delay
        vgmstream->loop_end_sample = atrac3_bytes_to_samples(loop_end, block_align) - encoder_delay;
    }
#endif

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Android/iOS Variants (Star Ocean Anamnesis (APK v1.9.2), Heaven x Inferno (iOS)) */
VGMSTREAM * init_vgmstream_ta_aac_mobile_vorbis(STREAMFILE *streamFile) {
#ifdef VGM_USE_VORBIS
    off_t start_offset;
    int8_t codec_id;

    /* check extension, case insensitive */
    /* .aac: expected, .laac/ace: for players to avoid hijacking MP4/AAC */
    if (!check_extensions(streamFile, "aac,laac,ace"))
        goto fail;

    if (read_32bitLE(0x00, streamFile) != 0x41414320)   /* "AAC " */
        goto fail;

    if (read_32bitLE(0xf0, streamFile) != 0x57415645)   /* "WAVE" */
        goto fail;

    codec_id = read_8bit(0x104, streamFile);
    if (codec_id == 0xe) /* Vorbis */
    {
        ogg_vorbis_meta_info_t ovmi = {0};
        VGMSTREAM * result = NULL;

        ovmi.meta_type = meta_TA_AAC_MOBILE;
        ovmi.loop_start = read_32bitLE(0x140, streamFile);
        ovmi.loop_end = read_32bitLE(0x144, streamFile);
        ovmi.loop_flag = ovmi.loop_end > ovmi.loop_start;
        ovmi.loop_end_found = ovmi.loop_flag;

        start_offset = read_32bitLE(0x120, streamFile);
        result = init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, start_offset, &ovmi);

        if (result != NULL) {
            return result;
        }
    }

fail:
    /* clean up anything we may have opened */
#endif
    return NULL;
}

/* Android/iOS Variants, before they switched to Vorbis (Star Ocean Anamnesis (Android), Heaven x Inferno (iOS)) */
VGMSTREAM * init_vgmstream_ta_aac_mobile(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, codec;
    size_t data_size;


    /* check extension, case insensitive */
    /* .aac: expected, .laac: for players to avoid hijacking MP4/AAC */
    if (!check_extensions(streamFile, "aac,laac"))
        goto fail;

    if (read_32bitLE(0x00, streamFile) != 0x41414320)   /* "AAC " */
        goto fail;

    if (read_32bitLE(0xf0, streamFile) != 0x57415645)   /* "WAVE" */
        goto fail;

    codec = read_8bit(0x104, streamFile);
    channel_count = read_8bit(0x105, streamFile);
    /* 0x106: 0x01?, 0x107: 0x10? */
    data_size = read_32bitLE(0x10c, streamFile); /* usable data only, cuts last frame */
    start_offset = read_32bitLE(0x120, streamFile);
    /* 0x124: full data size */
    loop_flag = (read_32bitLE(0x134, streamFile) > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x108, streamFile);
    vgmstream->meta_type = meta_TA_AAC_MOBILE;

    switch(codec) {
        case 0x0d:
            if (read_32bitLE(0x144, streamFile) != 0x40) goto fail; /* frame size */
            /* 0x148 or 0x150 (later games): frame samples */
            if (channel_count > 2) goto fail; /* unknown data layout */

            vgmstream->coding_type = coding_ASKA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = aska_bytes_to_samples(data_size, channel_count);
            vgmstream->loop_start_sample = aska_bytes_to_samples(read_32bitLE(0x130, streamFile), channel_count);
            vgmstream->loop_end_sample = aska_bytes_to_samples(read_32bitLE(0x134, streamFile), channel_count);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Vita variants [Judas Code (Vita)] */
VGMSTREAM * init_vgmstream_ta_aac_vita(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;


    /* check extension, case insensitive */
    /* .aac: expected, .laac: for players to avoid hijacking MP4/AAC */
    if (!check_extensions(streamFile, "aac,laac"))
        goto fail;

    if (read_32bitLE(0x00, streamFile) != 0x41414320)   /* "AAC " */
        goto fail;
    if (read_32bitLE(0x14, streamFile) != 0x56495441)   /* "VITA" */
        goto fail;
    if (read_32bitLE(0x10d0, streamFile) != 0x57415645)   /* "WAVE" */
        goto fail;

    /* there is a bunch of chunks but we simplify */

    /* 0x10E4: codec 0x08? */
    channel_count = read_8bit(0x10E5, streamFile);
    start_offset = read_32bitLE(0x1100, streamFile);
    loop_flag = (read_32bitLE(0x1114, streamFile) > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10e8, streamFile);
    vgmstream->meta_type = meta_TA_AAC_VITA;

#ifdef VGM_USE_ATRAC9
    {
        atrac9_config cfg = {0};

        cfg.channels = vgmstream->channels;
        cfg.encoder_delay = read_32bitLE(0x1124,streamFile);
        cfg.config_data = read_32bitBE(0x1128,streamFile);

        vgmstream->codec_data = init_atrac9(&cfg);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_ATRAC9;
        vgmstream->layout_type = layout_none;

        vgmstream->num_samples = atrac9_bytes_to_samples(read_32bitLE(0x10EC, streamFile), vgmstream->codec_data);
        vgmstream->num_samples -= cfg.encoder_delay;
        vgmstream->loop_start_sample = atrac9_bytes_to_samples(read_32bitLE(0x1110, streamFile), vgmstream->codec_data);
        vgmstream->loop_end_sample = atrac9_bytes_to_samples(read_32bitLE(0x1114, streamFile), vgmstream->codec_data);
    }
#endif

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
