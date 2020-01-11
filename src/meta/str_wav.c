#include "meta.h"
#include "../coding/coding.h"


typedef enum { PSX, DSP, XBOX, WMA, IMA, XMA2 } strwav_codec;
typedef struct {
    int32_t channels;
    int32_t sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int32_t loop_flag;
    size_t interleave;

    off_t coefs_offset;
    off_t dsps_table;
    off_t coefs_table;

    uint32_t flags;
    strwav_codec codec;
} strwav_header;

static int parse_header(STREAMFILE* streamHeader, strwav_header* strwav);


/* STR+WAV - Blitz Games streams+header */
VGMSTREAM * init_vgmstream_str_wav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    strwav_header strwav = {0};
    off_t start_offset;


    /* checks */
    if (!check_extensions(streamFile, "str,data"))
        goto fail;

    /* get external header (extracted with filenames from bigfiles) */
    {
        /* try body+header combos:
         * - file.wav.str=body + file.wav=header [common]
         * - file.wma.str + file.wma [Fuzion Frenzy (Xbox)]
         * - file.data + file (extensionless) [SpongeBob's Surf & Skate Roadtrip (X360)] */
        char basename[PATH_LIMIT];
        get_streamfile_basename(streamFile,basename,PATH_LIMIT);
        streamHeader = open_streamfile_by_filename(streamFile, basename);
        if (!streamHeader) {
            /* try again with file.str=body + file.wav=header [Bad Boys II (PS2), Zapper (PS2)] */
            streamHeader = open_streamfile_by_ext(streamFile, "wav");
            if (!streamHeader) {
                /* try again with file.str=body + file.sth=header (renamed/fake extension) */
                streamHeader = open_streamfile_by_ext(streamFile, "sth");
                if (!streamHeader) goto fail;
            }
        }
        else {
            /* header must have known extensions */
            if (!check_extensions(streamHeader, "wav,wma,"))
                goto fail;
        }
    }

    /* detect version */
    if (!parse_header(streamHeader, &strwav))
        goto fail;

    if (strwav.flags == 0)
        goto fail;
    if (strwav.channels > 8)
        goto fail;

    /* &0x01: loop?, &0x02: non-mono?, &0x04: stream???, &0x08: unused? */
    if (strwav.flags > 0x07) {
        VGM_LOG("STR+WAV: unknown flags\n");
        goto fail;
    }


    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(strwav.channels,strwav.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = strwav.sample_rate;
    vgmstream->num_samples = strwav.num_samples;
    if (strwav.loop_flag) {
        vgmstream->loop_start_sample = strwav.loop_start;
        vgmstream->loop_end_sample = strwav.loop_end;
    }

    vgmstream->meta_type = meta_STR_WAV;

    switch(strwav.codec) {
        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = strwav.interleave;
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = strwav.interleave;

            /* get coefs */
            {
                int i, ch;
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    for (i = 0; i < 16; i++) {
                        off_t coef_offset;
                        if (strwav.dsps_table) /* mini table with offsets to DSP headers */
                            coef_offset = read_32bitBE(strwav.dsps_table+0x04*ch,streamHeader) + 0x1c;
                        else if (strwav.coefs_table) /* mini table with offsets to coefs then header */
                            coef_offset = read_32bitBE(strwav.coefs_table+0x04*ch,streamHeader);
                        else
                            coef_offset = strwav.coefs_offset + 0x60*ch;
                        vgmstream->ch[ch].adpcm_coef[i] = read_16bitBE(coef_offset+i*0x02,streamHeader);
                    }
                }
            }
            break;

        case XBOX:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_interleave; /* interleaved stereo for >2ch*/
            vgmstream->interleave_block_size = strwav.interleave;
            if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
                goto fail; /* only 2ch+..+2ch layout is known */
            break;

        case IMA:
            vgmstream->coding_type = coding_BLITZ_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = strwav.interleave;
            break;

#ifdef VGM_USE_FFMPEG
        case WMA: {
            ffmpeg_codec_data *ffmpeg_data = NULL;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,get_streamfile_size(streamFile));
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ffmpeg_data->totalSamples;
            if (vgmstream->channels != ffmpeg_data->channels)
                goto fail;

            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            uint8_t buf[0x100];
            size_t stream_size;
            size_t bytes, block_size, block_count;

            stream_size = get_streamfile_size(streamFile);
            block_size = 0x10000;
            block_count = stream_size / block_size; /* not accurate? */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, strwav.num_samples, stream_size, strwav.channels, strwav.sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, 0x00,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, streamFile, 0x00,stream_size, 0, 0,0);
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}



/* Parse header versions. Almost every game uses its own variation (struct serialization?),
 * so detection could be improved once enough versions are known. */
static int parse_header(STREAMFILE* streamHeader, strwav_header* strwav) {
    size_t header_size;

    if (read_32bitBE(0x00,streamHeader) != 0x00000000)
        goto fail;

    header_size = get_streamfile_size(streamHeader);

    //todo loop start/end values may be off for some headers

    /* Fuzion Frenzy (Xbox)[2001] wma */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitLE(0x20,streamHeader) == 0  && /* no num_samples */
         read_32bitLE(0x24,streamHeader) == read_32bitLE(0x80,streamHeader) && /* sample rate repeat */
         read_32bitLE(0x28,streamHeader) == 0x10 &&
         header_size == 0x110 /* no value in header */
         ) {
        strwav->num_samples = read_32bitLE(0x20,streamHeader); /* 0 but will be rectified later */
        strwav->sample_rate = read_32bitLE(0x24,streamHeader);
        strwav->flags       = read_32bitLE(0x2c,streamHeader);
        strwav->loop_start  = 0;
        strwav->loop_end    = 0;

        strwav->channels    = read_32bitLE(0x60,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = 0;

        strwav->codec = WMA;
        //;VGM_LOG("STR+WAV: header Fuzion Frenzy (Xbox)\n");
        return 1;
    }

    /* Taz: Wanted (GC)[2002] */
    /* Cubix Robots for Everyone: Showdown (GC)[2003] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitBE(0x24,streamHeader) == read_32bitBE(0x90,streamHeader) && /* sample rate repeat */
         read_32bitBE(0x28,streamHeader) == 0x10 &&
         read_32bitBE(0xa0,streamHeader) == header_size /* ~0x3C0 */
         ) {
        strwav->num_samples = read_32bitBE(0x20,streamHeader);
        strwav->sample_rate = read_32bitBE(0x24,streamHeader);
        strwav->flags       = read_32bitBE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitBE(0xb8,streamHeader);
        strwav->loop_end    = read_32bitBE(0xbc,streamHeader);

        strwav->channels    = read_32bitBE(0x50,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x8000 : 0x10000;

        strwav->coefs_offset = 0xdc;
        strwav->codec = DSP;
        //;VGM_LOG("STR+WAV: header Taz: Wanted (GC)\n");
        return 1;
    }

    /* The Fairly OddParents - Breakin' da Rules (Xbox)[2003] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitLE(0x24,streamHeader) == read_32bitLE(0xb0,streamHeader) && /* sample rate repeat */
         read_32bitLE(0x28,streamHeader) == 0x10 &&
         read_32bitLE(0xc0,streamHeader)*0x04 + read_32bitLE(0xc4,streamHeader) == header_size /* ~0xe0 + variable */
         ) {
        strwav->num_samples = read_32bitLE(0x20,streamHeader);
        strwav->sample_rate = read_32bitLE(0x24,streamHeader);
        strwav->flags       = read_32bitLE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitLE(0x38,streamHeader);
        strwav->loop_end    = strwav->num_samples;

        strwav->channels    = read_32bitLE(0x70,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = 0xD800/2;

        strwav->codec = XBOX;
        //;VGM_LOG("STR+WAV: header The Fairly OddParents (Xbox)\n");
        return 1;
    }

    /* Bad Boys II (GC)[2004] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000800 &&
         read_32bitBE(0x24,streamHeader) == read_32bitBE(0xb0,streamHeader) && /* sample rate repeat */
         read_32bitBE(0x24,streamHeader) == read_32bitBE(read_32bitBE(0xe0,streamHeader)+0x08,streamHeader) && /* sample rate vs 1st DSP header */
         read_32bitBE(0x28,streamHeader) == 0x10 &&
         read_32bitBE(0xc0,streamHeader)*0x04 + read_32bitBE(0xc4,streamHeader) == header_size /* variable + variable */
         ) {
        strwav->num_samples = read_32bitBE(0x20,streamHeader);
        strwav->sample_rate = read_32bitBE(0x24,streamHeader);
        strwav->flags       = read_32bitBE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitBE(0xd8,streamHeader);
        strwav->loop_end    = read_32bitBE(0xdc,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader) * read_32bitBE(0x88,streamHeader); /* tracks of Nch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x8000 : 0x10000;

        strwav->dsps_table = 0xe0;
        strwav->codec = DSP;
        //;VGM_LOG("STR+WAV: header Bad Boys II (GC)\n");
        return 1;
    }

    /* Bad Boys II (PS2)[2004] */
    /* Pac-Man World 3 (PS2)[2005] */
    if ((read_32bitBE(0x04,streamHeader) == 0x00000800 ||
            read_32bitBE(0x04,streamHeader) == 0x01000800) && /* rare (PW3 mu_spectral1_explore_2) */
         read_32bitLE(0x24,streamHeader) == read_32bitLE(0x70,streamHeader) && /* sample rate repeat */
         read_32bitLE(0x28,streamHeader) == 0x10 &&
         read_32bitLE(0x78,streamHeader)*0x04 + read_32bitLE(0x7c,streamHeader) == header_size /* ~0xe0 + variable */
         ) {
        strwav->num_samples = read_32bitLE(0x20,streamHeader);
        strwav->sample_rate = read_32bitLE(0x24,streamHeader);
        strwav->flags       = read_32bitLE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitLE(0x38,streamHeader);
        strwav->interleave  = read_32bitLE(0x40,streamHeader) == 1 ? 0x8000 : 0x4000;
        strwav->loop_end    = read_32bitLE(0x54,streamHeader);

        strwav->channels    = read_32bitLE(0x40,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        //;VGM_LOG("STR+WAV: header Bad Boys II (PS2)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (PS2)[2005] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitLE(0x24,streamHeader) == read_32bitLE(0x70,streamHeader) && /* sample rate repeat */
         read_32bitLE(0x28,streamHeader) == 0x10 &&
         read_32bitLE(0x7c,streamHeader) == header_size /* ~0xD0 */
         ) {
        strwav->num_samples = read_32bitLE(0x20,streamHeader);
        strwav->sample_rate = read_32bitLE(0x24,streamHeader);
        strwav->flags       = read_32bitLE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitLE(0x38,streamHeader);
        strwav->loop_end    = read_32bitLE(0x54,streamHeader);

        strwav->channels    = read_32bitLE(0x40,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        //;VGM_LOG("STR+WAV: header Zapper (PS2)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (GC)[2005] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitBE(0x24,streamHeader) == read_32bitBE(0xB0,streamHeader) && /* sample rate repeat */
         read_32bitBE(0x28,streamHeader) == 0x10 &&
         read_32bitLE(0xc0,streamHeader) == header_size /* variable LE size */
         ) {
        strwav->num_samples = read_32bitBE(0x20,streamHeader);
        strwav->sample_rate = read_32bitBE(0x24,streamHeader);
        strwav->flags       = read_32bitBE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitBE(0xd8,streamHeader);
        strwav->loop_end    = read_32bitBE(0xdc,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader) * read_32bitBE(0x88,streamHeader); /* tracks of Nch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x8000 : 0x10000;

        strwav->dsps_table = 0xe0;
        strwav->codec = DSP;
        //;VGM_LOG("STR+WAV: header Zapper (GC)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (PC)[2005] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000900 &&
         read_32bitLE(0x24,streamHeader) == read_32bitLE(0x114,streamHeader) && /* sample rate repeat */
         read_32bitLE(0x28,streamHeader) == 0x10 &&
         read_32bitLE(0x12c,streamHeader) == header_size /* ~0x130 */
         ) {
        strwav->num_samples = read_32bitLE(0x20,streamHeader);
        strwav->sample_rate = read_32bitLE(0x24,streamHeader);
        strwav->flags       = read_32bitLE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitLE(0x54,streamHeader);
        strwav->loop_end    = read_32bitLE(0x30,streamHeader);

        strwav->channels    = read_32bitLE(0xF8,streamHeader) * (strwav->flags & 0x02 ? 2 : 1); /* tracks of 2/1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x8000 : 0x10000;

        strwav->codec = IMA;
        //;VGM_LOG("STR+WAV: header Zapper (PC)\n");
        return 1;
    }

    /* Pac-Man World 3 (GC)[2005] */
    /* SpongeBob SquarePants: Creature from the Krusty Krab (GC)[2006] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000800 &&
         read_32bitBE(0x24,streamHeader) == read_32bitBE(0xb0,streamHeader) && /* sample rate repeat */
         read_32bitBE(0x24,streamHeader) == read_32bitBE(read_32bitBE(0xf0,streamHeader)+0x08,streamHeader) && /* sample rate vs 1st DSP header */
         read_32bitBE(0x28,streamHeader) == 0x10 &&
         read_32bitBE(0xc0,streamHeader)*0x04 + read_32bitBE(0xc4,streamHeader) == read_32bitBE(0xe0,streamHeader) && /* main size */
        (read_32bitBE(0xe0,streamHeader) + read_32bitBE(0xe4,streamHeader)*0x40 == header_size || /* main size + extradata 1 (config? PMW3 cs2.wav) */
         read_32bitBE(0xe0,streamHeader) + read_32bitBE(0xe4,streamHeader)*0x08 == header_size) /* main size + extradata 2 (ids? SBSP 0_0_mu_hr.wav) */
         ) {
        strwav->num_samples = read_32bitBE(0x20,streamHeader);
        strwav->sample_rate = read_32bitBE(0x24,streamHeader);
        strwav->flags       = read_32bitBE(0x2c,streamHeader);
        strwav->loop_start  = read_32bitBE(0xd8,streamHeader);
        strwav->loop_end    = read_32bitBE(0xdc,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader) * read_32bitBE(0x88,streamHeader); /* tracks of Nch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 2 ? 0x8000 : 0x10000;

        strwav->dsps_table = 0xf0;
        strwav->codec = DSP;
        //;VGM_LOG("STR+WAV: header SpongeBob SquarePants (GC)\n");
        return 1;
    }

    /* SpongeBob SquarePants: Creature from the Krusty Krab (PS2)[2006] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000800 &&
         read_32bitLE(0x08,streamHeader) == 0x00000000 &&
         read_32bitLE(0x0c,streamHeader) != header_size && /* some ID */
         (header_size == 0x74 + read_32bitLE(0x64,streamHeader)*0x04 ||
                 header_size == 0x78 + read_32bitLE(0x64,streamHeader)*0x04)
         ) {
        strwav->loop_start  = read_32bitLE(0x24,streamHeader); //not ok?
        strwav->num_samples = read_32bitLE(0x30,streamHeader);
        strwav->loop_end    = read_32bitLE(0x34,streamHeader);
        strwav->sample_rate = read_32bitLE(0x38,streamHeader);
        strwav->flags       = read_32bitLE(0x3c,streamHeader);

        strwav->channels    = read_32bitLE(0x64,streamHeader); /* tracks of 1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        //;VGM_LOG("STR+WAV: header SpongeBob SquarePants (PS2)\n");
        return 1;
    }

    /* Tak and the Guardians of Gross (PS2)[2008] */
    if ( read_32bitBE(0x04,streamHeader) == 0x00000800 &&
         read_32bitLE(0x08,streamHeader) != 0x00000000 &&
         read_32bitLE(0x0c,streamHeader) == header_size && /* ~0x80+0x04*ch */
         read_32bitLE(0x0c,streamHeader) == 0x80 + read_32bitLE(0x70,streamHeader)*0x04
         ) {
        strwav->loop_start  = read_32bitLE(0x24,streamHeader); //not ok?
        strwav->num_samples = read_32bitLE(0x30,streamHeader);
        strwav->loop_end    = read_32bitLE(0x34,streamHeader);
        strwav->sample_rate = read_32bitLE(0x38,streamHeader);
        strwav->flags       = read_32bitLE(0x3c,streamHeader);

        strwav->channels    = read_32bitLE(0x70,streamHeader); /* tracks of 1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        //;VGM_LOG("STR+WAV: header Tak (PS2)\n");
        return 1;
    }

    /* Tak and the Guardians of Gross (Wii)[2008] */
    /* The House of the Dead: Overkill (Wii)[2009] (not Blitz but still the same format) */
    if ((read_32bitBE(0x04,streamHeader) == 0x00000800 ||
            read_32bitBE(0x04,streamHeader) == 0x00000700) && /* rare? */
         read_32bitLE(0x08,streamHeader) != 0x00000000 &&
         read_32bitBE(0x0c,streamHeader) == header_size && /* variable per header */
         read_32bitBE(0x7c,streamHeader) != 0 && /* has DSP header */
         read_32bitBE(0x38,streamHeader) == read_32bitBE(read_32bitBE(0x7c,streamHeader)+0x38,streamHeader) /* sample rate vs 1st DSP header */
         ) {
        strwav->loop_start  = 0; //read_32bitLE(0x24,streamHeader); //not ok?
        strwav->num_samples = read_32bitBE(0x30,streamHeader);
        strwav->loop_end    = read_32bitBE(0x34,streamHeader);
        strwav->sample_rate = read_32bitBE(0x38,streamHeader);
        strwav->flags       = read_32bitBE(0x3c,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader); /* tracks of 1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;

        strwav->coefs_table = 0x7c;
        strwav->codec = DSP;
        //;VGM_LOG("STR+WAV: header Tak/HOTD:O (Wii)\n");
        return 1;
    }

    /* The House of the Dead: Overkill (PS3)[2009] (not Blitz but still the same format) */
    if ((read_32bitBE(0x04,streamHeader) == 0x00000800 ||
            read_32bitBE(0x04,streamHeader) == 0x00000700) && /* rare? */
         read_32bitLE(0x08,streamHeader) != 0x00000000 &&
         read_32bitBE(0x0c,streamHeader) == header_size && /* variable per header */
         read_32bitBE(0x7c,streamHeader) == 0 /* not DSP header */
         ) {
        strwav->loop_start  = 0; //read_32bitLE(0x24,streamHeader); //not ok?
        strwav->num_samples = read_32bitBE(0x30,streamHeader);
        strwav->loop_end    = read_32bitBE(0x34,streamHeader);
        strwav->sample_rate = read_32bitBE(0x38,streamHeader);
        strwav->flags       = read_32bitBE(0x3c,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader); /* tracks of 1ch */
        strwav->loop_flag   = strwav->flags & 0x01;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        //;VGM_LOG("STR+WAV: header HOTD:O (PS3)\n");
        return 1;
    }

    /* SpongeBob's Surf & Skate Roadtrip (X360)[2011] */
    if ((read_32bitBE(0x04,streamHeader) == 0x00000800 || /* used? */
            read_32bitBE(0x04,streamHeader) == 0x00000700) &&
         read_32bitLE(0x08,streamHeader) != 0x00000000 &&
         read_32bitBE(0x0c,streamHeader) == 0x124 && /* variable, not sure about final calc */
         read_32bitBE(0x8c,streamHeader) == 0x180 /* encoder delay actually */
         //0x4c is data_size + 0x210
         ) {
        strwav->loop_start  = 0; //read_32bitLE(0x24,streamHeader); //not ok?
        strwav->num_samples = read_32bitBE(0x30,streamHeader);//todo sometimes wrong?
        strwav->loop_end    = read_32bitBE(0x34,streamHeader);
        strwav->sample_rate = read_32bitBE(0x38,streamHeader);
        strwav->flags       = read_32bitBE(0x3c,streamHeader);

        strwav->channels    = read_32bitBE(0x70,streamHeader); /* multichannel XMA */
        strwav->loop_flag   = strwav->flags & 0x01;

        strwav->codec = XMA2;
        //;VGM_LOG("STR+WAV: header SBSSR (X360)\n");
        return 1;
    }

    /* unknown */
    goto fail;

fail:
    return 0;
}
