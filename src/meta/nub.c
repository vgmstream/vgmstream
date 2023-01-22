#include "meta.h"
#include "../coding/coding.h"


static void load_name(char* name, size_t name_size, STREAMFILE* sf, int big_endian, int total_subsongs, int target_subsong);
static STREAMFILE* setup_nub_streamfile(STREAMFILE *sf, off_t header_offset, size_t header_size, off_t stream_offset, size_t stream_size, const char* fake_ext);

/* .nub - Namco's nuSound2 audio container */
VGMSTREAM* init_vgmstream_nub(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int big_endian;
    int total_subsongs, target_subsong = sf->stream_index;
    uint32_t version, codec;
    const char* fake_ext;
    VGMSTREAM*(*init_vgmstream_function)(STREAMFILE*) = NULL;
    char name[STREAM_NAME_SIZE] = {0};
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .nub: standard
     * .nub2: rare [iDOLM@STER - Gravure For You (PS3)] */
    if (!check_extensions(sf, "nub,nub2"))
        goto fail;

    version = read_32bitBE(0x00,sf);
    if (version != 0x00020000 &&  /* v2.0 (rare, ex. Ridge Race 6 (X360)) */
        version != 0x00020100 &&  /* v2.1 (common) */
        version != 0x01020100)    /* same but LE (seen in PSP/PC games, except PS4) */
        goto fail;
    if (read_32bitBE(0x04,sf) != 0x00000000) /* null */
        goto fail;

    /* sometimes LE [Soul Calibur: Broken Destiny (PSP), Tales of Vesperia (PS4) */
    big_endian = guess_endianness32bit(0x18, sf);
    if (big_endian) {
        read_32bit = read_32bitBE;
    } else{
        read_32bit = read_32bitLE;
    }

    /* parse TOC */
    {
        off_t offset, data_start, header_start;
        off_t header_offset, subheader_start, stream_offset;
        size_t header_size, subheader_size, stream_size;

        /* - base header */
        /* 0x08: file id (0 = first) */
        total_subsongs = read_32bit(0x0c, sf); /* .nub with 0 files do exist, and with 1 song only too */
        data_start = read_32bit(0x10, sf); /* exists even with 0 files */
        /* 0x14: data end (may have padding) */
        header_start = read_32bit(0x18, sf); /* exists even with 0 files */
        /* 0x1c: header end */


        /* probably means "header end" in v2.0 */
        if (version == 0x00020000) {
            data_start = align_size_to_block(data_start, 0x800);
        }

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        offset = read_32bit(header_start + (target_subsong-1)*0x04, sf);

        /* .nus have all headers first then all data, but extractors often just paste them together,
         * so we'll combine header+data on the fly to make them playable with existing parsers.
         * Formats inside .nub normally don't exist as external files, so they could be extracted in various
         * ways that we'll try to match (though IDSP/BNSF can be found as header+data in some bigfiles).
         * Headers seem to be called "tones" and data "streams" in debug strings. */

        header_offset = offset;

        /* - extension (as referenced in companion files with internal filenames, ex. "BGM_MovingDemo1.is14" > "is14") */
        if (version != 0x00020000)
            offset += 0x04; /* skip, not found in v2.0 */

        /* - tone header */
        /* 0x00: config? */
        /* 0x04: header id/number */
        codec = (uint32_t)read_32bit(offset + 0x08, sf);
        /* 0x0c: null */
        stream_size    = read_32bit(offset + 0x10, sf); /* 0x10 aligned */
        stream_offset  = read_32bit(offset + 0x14, sf) + data_start;
        subheader_size = read_32bit(offset + 0x18, sf);
        /* 0x1c: extra info, size 0x10 (meaning varies but usually loop points) */
        /* rest until sub-header start looks like config/volumes/pan/etc in various floats */

        if (version == 0x00020000)
            subheader_start = 0xAC;
        else
            subheader_start = 0xBC;
        header_size = align_size_to_block(subheader_start + subheader_size, 0x10);

        switch(codec) {
            case 0x00: /* (none) (xma1) */
                fake_ext = "xma";
                init_vgmstream_function = init_vgmstream_nub_xma;
                break;

            case 0x01: /* "wav\0" */
                fake_ext = "wav";
                init_vgmstream_function = init_vgmstream_nub_wav;
                break;

            case 0x02: /* "vag\0" */
                fake_ext = "vag";
                init_vgmstream_function = init_vgmstream_nub_vag;
                break;

            case 0x03: /* "at3\0" */
                fake_ext = "at3";
                init_vgmstream_function = init_vgmstream_nub_at3;
                break;

            case 0x04: /* "xma\0" (xma2 old) */
            case 0x08: /* "xma\0" (xma2 new) */
                fake_ext = "xma";
                init_vgmstream_function = init_vgmstream_nub_xma;
                break;

            case 0x05: /* "dsp\0" */
                fake_ext = "dsp";
                init_vgmstream_function = init_vgmstream_nub_dsp;
                break;

            case 0x06: /* "idsp" */
                fake_ext = "idsp";
                init_vgmstream_function = init_vgmstream_nub_idsp;
                break;

            case 0x07: /* "is14" */
                fake_ext = "is14";
                init_vgmstream_function = init_vgmstream_nub_is14;
                break;

            default:
                VGM_LOG("NUB: unknown codec %x\n", codec);
                goto fail;
        }

        //;VGM_LOG("NUB: subfile header=%lx + %x, offset=%lx + %x\n", header_offset, header_size, stream_offset, stream_size);

        temp_sf = setup_nub_streamfile(sf, header_offset, header_size, stream_offset, stream_size, fake_ext);
        if (!temp_sf) goto fail;
    }


    /* get names from companion file, rarely [Noby Noby Boy (PS3), Wangan Midnight Maximum Tune (AC)] */
    load_name(name, sizeof(name), sf, big_endian, total_subsongs, target_subsong);


    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_function(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->stream_size = get_streamfile_size(temp_sf);
    vgmstream->num_streams = total_subsongs;
    if (name[0] != '\0')
        strcpy(vgmstream->stream_name, name);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* *********************************************************** */

static void load_name(char* name, size_t name_size, STREAMFILE* sf, int big_endian, int total_subsongs, int target_subsong) {
    STREAMFILE *sf_names = NULL;
    char filename[PATH_LIMIT];
    char basename[255];
    char name1[0x40+1] = {0};
    char name2[0x40+1] = {0};
    int count;
    off_t offset;
    size_t name1_size, name2_size;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = big_endian ? read_u32be : read_u32le;

    get_streamfile_basename(sf, basename, sizeof(basename));
    snprintf(filename, sizeof(filename), "nuSound2ToneStr%s.bin", basename);

    sf_names = open_streamfile_by_filename(sf, filename);
    if (!sf_names) goto done;

    /* 0x00: version/endianness? (0=NNB, 1=WMMT5) */
    /* 0x04: version/endianness? (1=NNB, 0=WMMT5) */
    count = read_u32(0x08, sf_names);
    /* 0x0c: file size */
    name1_size = read_u32(0x10, sf_names);
    name2_size = read_u32(0x14, sf_names);
    /* 0x18/1c: null */
    /* 0x20: bank name (size 0x20) */

    if (count != total_subsongs)
        goto done;
    if (name1_size >= sizeof(name1) || name2_size >= sizeof(name2))
        goto done;

    offset = 0x40 + (target_subsong - 1) * (name1_size + name2_size);

    read_string(name1, name1_size, offset + 0x00, sf_names); /* internal name */
    read_string(name2, name2_size, offset + name1_size, sf_names); /* file name */
    //todo some filenames use shift-jis, not sure what to do

    snprintf(name, name_size, "%s/%s", name1, name2);

done:
    close_streamfile(sf_names);
}


static STREAMFILE* setup_nub_streamfile(STREAMFILE* sf, off_t header_offset, size_t header_size, off_t stream_offset, size_t stream_size, const char* fake_ext) {
    STREAMFILE* new_sf = NULL;
    STREAMFILE* multi_sf[2] = {0};

    multi_sf[0] = open_wrap_streamfile(sf);
    multi_sf[0] = open_clamp_streamfile_f(multi_sf[0], header_offset, header_size);
    multi_sf[1] = open_wrap_streamfile(sf);
    multi_sf[1] = open_clamp_streamfile_f(multi_sf[1], stream_offset, stream_size);
    new_sf = open_multifile_streamfile_f(multi_sf, 2);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, fake_ext);
    return new_sf;
}

/* *********************************************************** */

//todo could be simplified

/* .nub wav - from Namco NUB archives [Ridge Racer 7 (PS3)] */
VGMSTREAM* init_vgmstream_nub_wav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size, loop_start, loop_length;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(sf, "wav,lwav"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x77617600) /* "wav\0" "*/
        goto fail;

    if (guess_endianness32bit(0x1c, sf)) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    data_size   = read_32bit(0x14,sf);
    /* info header */
    loop_start  = read_32bit(0x20,sf);
    loop_length = read_32bit(0x24,sf);
    loop_flag   = read_32bit(0x28,sf);
    /* 0x2c: null */

    /* format header: mini "fmt" chunk */
    if (read_16bit(0xBC + 0x00, sf) != 0x0001)
        goto fail;
    channel_count = read_16bit(0xBC + 0x02,sf);
    sample_rate  = read_32bit(0xBC + 0x04,sf);
    /* 0x08: bitrate */
    /* 0x0c: block align/bps */

    start_offset = 0xD0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NUB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, 16);
    vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channel_count, 16);
    vgmstream->loop_end_sample   = pcm_bytes_to_samples(loop_start + loop_length, channel_count, 16);

    vgmstream->coding_type = coding_PCM16BE; /* always BE */
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .nub vag - from Namco NUB archives [Ridge Racer 7 (PS3), Noby Noby Boy (PS3)] */
VGMSTREAM* init_vgmstream_nub_vag(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size, loop_start, loop_length;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if ( !check_extensions(sf, "vag"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x76616700) /* "vag\0" */
        goto fail;

    if (guess_endianness32bit(0x1c, sf)) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    data_size   = read_32bit(0x14,sf);
    /* info header */
    loop_start  = read_32bit(0x20,sf);
    loop_length = read_32bit(0x24,sf);
    loop_flag   = read_32bit(0x28,sf);
    /* 0x2c: null */

    /* format header */
    sample_rate  = read_32bit(0xBC + 0x00,sf);

    channel_count = 1;
    start_offset = 0xC0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NUB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channel_count);
    vgmstream->loop_end_sample   = ps_bytes_to_samples(loop_start + loop_length, channel_count);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->allow_dual_stereo = 1;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .nub at3 - from Namco NUB archives [Ridge Racer 7 (PS3), Katamari Forever (PS3)] */
VGMSTREAM* init_vgmstream_nub_at3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset = 0;
    size_t subfile_size = 0;


    /* checks */
    if (!check_extensions(sf,"at3"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x61743300) /* "at3\0" */
        goto fail;

    /* info header */
    /* 0x20: loop start (in samples) */
    /* 0x24: loop length (in samples) */
    /* 0x28: loop flag */
    /* 0x2c: null */

    /* format header: mini fmt (WAVEFORMATEX) + fact chunks LE (clone of RIFF's) */
    /* we can just ignore and use RIFF at data start since it has the same info */

    subfile_offset = 0x100;
    subfile_size   = read_32bitLE(subfile_offset + 0x04, sf) + 0x08; /* RIFF size */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_riff(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* .nub xma - from Namco NUB archives [Ridge Racer 6 (X360), Tekken 6 (X360), Galaga Legions DX (X360)] */
VGMSTREAM* init_vgmstream_nub_xma(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, chunk_offset;
    size_t data_size, chunk_size, header_size;
    int loop_flag, channel_count, sample_rate, nus_codec;
    int num_samples, loop_start_sample, loop_end_sample;


    /* checks */
    if (!check_extensions(sf,"xma"))
        goto fail;

    if (read_32bitBE(0x00,sf) == 0x786D6100) { /* "xma\0" */
        /* nub v2.1 */
        nus_codec    = read_32bitBE(0x0C,sf);
        data_size    = read_32bitBE(0x14,sf);
        header_size  = read_32bitBE(0x1c,sf);
        chunk_offset = 0xBC;

        /* info header */
        /* 0x20: null */
        chunk_size   = read_32bitBE(0x24,sf);
        /* 0x24: loop flag */
        /* 0x20: null */
    }
    else if (read_32bitBE(0x08,sf) == 0 && read_32bitBE(0x0c,sf) == 0) {
        /* nub v2.0 from Ridge Racer 6 */
        nus_codec    = read_32bitBE(0x08,sf);
        data_size    = read_32bitBE(0x10,sf);
        header_size  = read_32bitBE(0x18,sf);
        chunk_offset = 0xAC;

        chunk_size   = header_size;
    }
    else {
        goto fail;
    }

    start_offset = align_size_to_block(chunk_offset + header_size, 0x10);

    if (nus_codec == 0x00) { /* XMA1 "fmt " */
        int loop_start_b, loop_end_b, loop_subframe;

        xma1_parse_fmt_chunk(sf, chunk_offset, &channel_count,&sample_rate, &loop_flag, &loop_start_b, &loop_end_b, &loop_subframe, 1);

        {
            ms_sample_data msd = {0};

            msd.xma_version = 1;
            msd.channels    = channel_count;
            msd.data_offset = start_offset;
            msd.data_size   = data_size;
            msd.loop_flag   = loop_flag;
            msd.loop_start_b= loop_start_b;
            msd.loop_end_b  = loop_end_b;
            msd.loop_start_subframe = loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
            msd.loop_end_subframe   = loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */
            msd.chunk_offset= chunk_offset;

            xma_get_samples(&msd, sf);

            num_samples = msd.num_samples;
            loop_start_sample = msd.loop_start_sample;
            loop_end_sample = msd.loop_end_sample;
        }
    }
    else if (nus_codec == 0x04) { /* "XMA2" */
        xma2_parse_xma2_chunk(sf, chunk_offset, &channel_count,&sample_rate, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample);
    }
    else if (nus_codec == 0x08) { /* XMA2 "fmt " */
        channel_count = read_16bitBE(chunk_offset+0x02,sf);
        sample_rate   = read_32bitBE(chunk_offset+0x04,sf);
        xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);
    }
    else {
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NUB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1); /* samples needs adjustment */
    }
#else
    goto fail;
#endif

    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .nub dsp - from Namco NUB archives [Taiko no Tatsujin Wii Chou Goukanban (Wii)] */
VGMSTREAM* init_vgmstream_nub_dsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t header_offset, stream_offset;
    size_t header_size, stream_size;


    /* checks */
    if (!check_extensions(sf,"dsp"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x64737000)    /* "dsp\0" */
        goto fail;

    /* paste header+data together and pass to meta, which has loop info too */
    header_offset = 0xBC;
    stream_size = read_32bitBE(0x14, sf);
    header_size = read_32bitBE(0x1c, sf);
    stream_offset = align_size_to_block(header_offset + header_size, 0x10);

    temp_sf = setup_nub_streamfile(sf, header_offset, header_size, stream_offset, stream_size, "dsp");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ngc_dsp_std(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* .nub idsp - from Namco NUB archives [Soul Calibur Legends (Wii), Sky Crawlers: Innocent Aces (Wii)] */
VGMSTREAM* init_vgmstream_nub_idsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t header_offset, stream_offset;
    size_t header_size, stream_size;


    /* checks */
    if (!check_extensions(sf,"idsp"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x69647370)    /* "idsp" */
        goto fail;

    /* info header */
    /* 0x20: loop start (in samples) */
    /* 0x24: loop length (in samples) */
    /* 0x28: loop flag */
    /* 0x2c: null */

    /* paste header+data together and pass to meta, which has loop info too */
    header_offset = 0xBC;
    stream_size = read_32bitBE(0x14, sf);
    header_size = read_32bitBE(0x1c, sf);
    stream_offset = align_size_to_block(header_offset + header_size, 0x10);

    temp_sf = setup_nub_streamfile(sf, header_offset, header_size, stream_offset, stream_size, "idsp");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_idsp_namco(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* .nub is14 - from Namco NUB archives [Tales of Vesperia (PS3), Mojipittan (Wii)]  */
VGMSTREAM* init_vgmstream_nub_is14(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t header_offset, stream_offset;
    size_t header_size, stream_size, sdat_size;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(sf,"is14"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x69733134) /* "is14" */
        goto fail;

    if (guess_endianness32bit(0x1c, sf)) {
        read_32bit = read_32bitBE;
    } else{
        read_32bit = read_32bitLE;
    }

    /* info header: null (even when BSNF loops) */

    /* paste header+data together and pass to meta */
    header_offset = 0xBC;
    header_size = read_32bit(0x1c, sf);

    /* size at 0x14 is padded, find "sdat" size BE (may move around) */
    if (!find_chunk_riff_be(sf, 0x73646174, 0xbc+0x0c, header_size - 0x0c, NULL, &sdat_size))
        goto fail;
    stream_offset = align_size_to_block(header_offset + header_size, 0x10);
    stream_size   = sdat_size;


    temp_sf = setup_nub_streamfile(sf, header_offset, header_size, stream_offset, stream_size, "bnsf");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_bnsf(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
