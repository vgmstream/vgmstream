#include "meta.h"
#include "../coding/coding.h"


typedef enum { PSX, DSP, XBOX, WMA, IMA, XMA2 } strwav_codec;
typedef struct {
    int tracks;
    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;
    size_t interleave;

    off_t coefs_offset;
    off_t dsps_table;
    off_t coefs_table;

    uint32_t flags;
    strwav_codec codec;
} strwav_header;

static int parse_header(STREAMFILE* sf_h, STREAMFILE* sf_b, strwav_header* strwav);


/* STR+WAV - Blitz Games streams+header */
VGMSTREAM* init_vgmstream_str_wav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_h = NULL;
    strwav_header strwav = {0};
    off_t start_offset;


    /* checks */
    if (!check_extensions(sf, "str,data"))
        goto fail;

    /* get external header (extracted with filenames from bigfiles) */
    {
        /* try body+header combos:
         * - file.wav.str=body + file.wav=header [common]
         * - file.wma.str + file.wma [Fuzion Frenzy (Xbox)]
         * - file.data + file (extensionless) [SpongeBob's Surf & Skate Roadtrip (X360)] */
        char basename[PATH_LIMIT];
        get_streamfile_basename(sf,basename,PATH_LIMIT);
        sf_h = open_streamfile_by_filename(sf, basename);
        if (!sf_h) {
            /* try again with file.str=body + file.wav=header [Bad Boys II (PS2), Zapper (PS2)] */
            sf_h = open_streamfile_by_ext(sf, "wav");
            if (!sf_h) {
                /* try again with file.str=body + file.sth=header (renamed/fake extension) */
                sf_h = open_streamfile_by_ext(sf, "sth");
                if (!sf_h) goto fail;
            }
        }
        else {
            /* header must have known extensions */
            if (!check_extensions(sf_h, "wav,wma,"))
                goto fail;
        }
    }

    /* detect version */
    if (!parse_header(sf_h, sf, &strwav))
        goto fail;

    if (strwav.flags == 0)
        goto fail;
    /* &0x01: loop, &0x02: stereo tracks, &0x04: stream?, &0x200: has named cues? */
    if (strwav.flags & 0xFFFFFDF8) {
        VGM_LOG("STR+WAV: unknown flags %x\n", strwav.flags);
        goto fail;
    }

    strwav.loop_flag   = strwav.flags & 0x01;

    if (!strwav.channels)
        strwav.channels = strwav.tracks * (strwav.flags & 0x02 ? 2 : 1);
    if (strwav.channels > 8)
        goto fail;

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
                            coef_offset = read_32bitBE(strwav.dsps_table+0x04*ch,sf_h) + 0x1c;
                        else if (strwav.coefs_table) /* mini table with offsets to coefs then header */
                            coef_offset = read_32bitBE(strwav.coefs_table+0x04*ch,sf_h);
                        else
                            coef_offset = strwav.coefs_offset + 0x60*ch;
                        vgmstream->ch[ch].adpcm_coef[i] = read_16bitBE(coef_offset+i*0x02,sf_h);
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

            ffmpeg_data = init_ffmpeg_offset(sf, start_offset,get_streamfile_size(sf));
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ffmpeg_get_samples(ffmpeg_data);
            if (vgmstream->channels != ffmpeg_get_channels(ffmpeg_data))
                goto fail;

            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            uint8_t buf[0x100];
            size_t stream_size;
            size_t bytes, block_size, block_count;

            stream_size = get_streamfile_size(sf);
            block_size = 0x10000;
            block_count = stream_size / block_size; /* not accurate? */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, strwav.num_samples, stream_size, strwav.channels, strwav.sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, 0x00,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, 0x00,stream_size, 0, 0,0);
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    close_streamfile(sf_h);
    return vgmstream;

fail:
    close_streamfile(sf_h);
    close_vgmstream(vgmstream);
    return NULL;
}



/* Parse header versions. Almost every game uses its own variation (struct serialization?),
 * so detection could be improved once enough versions are known. */
static int parse_header(STREAMFILE* sf_h, STREAMFILE* sf_b, strwav_header* strwav) {
    size_t header_size;

    if (read_u32be(0x00,sf_h) != 0x00000000)
        goto fail;

    header_size = get_streamfile_size(sf_h);
    
    /* most variations have extra tables (at least 1 entry):
     * - table1: samples
     * - table2: f32 ms + cue hash (ex. 2D7FC4C5 = __EndMarker0, not unique) + optional 0x38 cue name
     * table entries don't need to match (table2 may be slightly bigger)
    */

    //todo loop start/end values may be off for some headers

    /* Fuzion Frenzy (Xbox)[2001] wma */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x0c,sf_h) != header_size &&
         read_u32le(0x24,sf_h) != 0 &&
         read_u32le(0x24,sf_h) == read_u32le(0x80,sf_h) && /* sample rate repeat */
         header_size == 0x110 /* no value in header */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h); /* 0 but will be rectified later */
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        /* 0x38: related to samples? */
        /* 0x48: number of chunks */
        strwav->tracks      = read_s32le(0x60,sf_h);
        /* 0x80: sample rate 2 */

        strwav->loop_start  = 0;
        strwav->loop_end    = 0;

        strwav->codec = WMA;
        strwav->interleave  = 0;
        ;VGM_LOG("STR+WAV: header FF (Xbox)\n");
        return 1;
    }

    /* Taz: Wanted (GC)[2002] */
    /* Cubix Robots for Everyone: Showdown (GC)[2003] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32be(0x0c,sf_h) != header_size &&
         read_u32be(0x24,sf_h) != 0 &&
         read_u32be(0x24,sf_h) == read_u32be(0x90,sf_h) && /* sample rate repeat */
         read_u32be(0xa0,sf_h) == header_size /* ~0x3C0 */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32be(0x20,sf_h);
        strwav->sample_rate = read_s32be(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32be(0x2c,sf_h);
        /* 0x38: related to samples? */
        strwav->tracks      = read_s32be(0x50,sf_h);
        /* 0x58: number of chunks */
        /* 0x90: sample rate 2 */
        /* 0xa0: header size */
        strwav->loop_start  = read_s32be(0xb8,sf_h);
        strwav->loop_end    = read_s32be(0xbc,sf_h);
        /* 0xc0: standard DSP header */

        strwav->codec = DSP;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;
        strwav->coefs_offset = 0xdc;
        ;VGM_LOG("STR+WAV: header TZW/CBX (GC)\n");
        return 1;
    }

    /* Taz Wanted demo (PC)[2003] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x24,sf_h) == read_u32le(0xfc,sf_h) && /* sample rate repeat */
         read_u32le(0x10c,sf_h) ==  header_size
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_end    = read_s32le(0x30,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        /* 0x58: number of chunks */
        strwav->tracks      = read_s32le(0xD8,sf_h);
        /* 0xfc: sample rate 2 */
        /* 0x100: ? */
        /* 0x10c: header size */

        strwav->codec = IMA;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;
        ;VGM_LOG("STR+WAV: header TAZd (PC)\n");
        return 1;
    }

    /* The Fairly OddParents - Breakin' da Rules (Xbox)[2003] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x24,sf_h) == read_u32le(0xb0,sf_h) && /* sample rate repeat */
         read_u32le(0xc0,sf_h)*0x04 + read_u32le(0xc4,sf_h) == header_size /* ~0xe0 + variable */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x70,sf_h);
        /* 0x78: number of chunks */
        /* 0xb0: sample rate 2 */
        /* 0xc0: table1 entries */
        /* 0xc4: table1 offset */
        /* 0xdc: total frames */

        strwav->loop_end    = strwav->num_samples;

        strwav->codec = XBOX;
        strwav->interleave  = strwav->tracks > 1 ? 0xD800/2 : 0xD800;
        ;VGM_LOG("STR+WAV: header FOP (Xbox)\n");
        return 1;
    }

    /* Pac-Man World 3 (Xbox)[2005] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 ||
         read_u32be(0x04,sf_h) == 0x01000800) && /* rare, mu_spectral1_explore_2 */
         read_u32le(0x24,sf_h) == read_u32le(0xB0,sf_h) && /* sample rate repeat */
         read_u32le(0x28,sf_h) == 0x10 &&
         read_u32le(0xE0,sf_h) + read_u32le(0xE4,sf_h) * 0x40 == header_size /* ~0x100 + cues */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x70,sf_h);
        /* 0x78: number of chunks */
        /* 0xb0: sample rate 2 */
        /* 0xdc: total frames */
        /* 0xe0: table2 offset */
        /* 0xe4: table2 entries */
        /* 0xf0: default hashname? */

        strwav->loop_end    = strwav->num_samples;

        strwav->codec = XBOX;
        strwav->interleave  = strwav->tracks > 1 ? 0xD800/2 : 0xD800;
        ;VGM_LOG("STR+WAV: PW3 (Xbox)\n");
        return 1;
    }

    /* The Fairly OddParents! - Shadow Showdown (GC)[2004] */
    /* Bad Boys II (GC)[2004] */
    if ( read_u32be(0x04,sf_h) == 0x00000800 &&
         read_u32be(0x24,sf_h) == read_u32be(0xb0,sf_h) && /* sample rate repeat */
         read_u32be(0x24,sf_h) == read_u32be(read_u32be(0xe0,sf_h)+0x08,sf_h) && /* sample rate vs 1st DSP header */
         read_u32be(0xc0,sf_h)*0x04 + read_u32be(0xc4,sf_h) == header_size /* variable + variable */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32be(0x20,sf_h);
        strwav->sample_rate = read_s32be(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32be(0x2c,sf_h);
        /* 0x38: related to samples? */
        strwav->tracks      = read_s32be(0x70,sf_h);
        /* 0x78: number of chunks */
        /* 0x88: track channels */
        /* 0xb0: sample rate 2 */
        /* 0xc0: table1 entries */
        /* 0xc4: table1 offset */

        strwav->loop_start  = read_s32be(0xd8,sf_h);
        strwav->loop_end    = read_s32be(0xdc,sf_h);
        /* 0xe0: DSP offset per channel */
        /* 0x100: standard DSP header */

        strwav->codec = DSP;
        strwav->dsps_table = 0xe0;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;
        ;VGM_LOG("STR+WAV: header FOP/BB2 (GC)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! Beta (GC)[2002] */
    /* Zapper: One Wicked Cricket! (GC)[2002] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32be(0x24,sf_h) == read_u32be(0xB0,sf_h) && /* sample rate repeat */
         read_u32be(0x88,sf_h) != 0 &&
         read_u32le(0xc0,sf_h) == header_size /* LE! */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32be(0x20,sf_h);
        strwav->sample_rate = read_s32be(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32be(0x2c,sf_h);
        /* 0x38: related to samples? */
        strwav->tracks      = read_s32be(0x70,sf_h);
        /* 0x78: number of chunks */
        /* 0x88: track channels */
        /* 0xC0: size */ 
        /* 0xb0: sample rate 2 */
        /* 0xc0: table1 offset LE */

        strwav->loop_start  = read_s32be(0xd8,sf_h);
        strwav->loop_end    = read_s32be(0xdc,sf_h);
        /* 0xe0: DSP offset per channel */
        /* 0x100: standard DSP header */

        strwav->codec = DSP;
        strwav->dsps_table = 0xe0;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;

        ;VGM_LOG("STR+WAV: header ZP (GC)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! Beta (PS2)[2002] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x2c,sf_h) == 44100 && /* sample rate */
         read_u32le(0x70,sf_h) == 0 && /* sample rate repeat? */
         header_size == 0x78
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        /* 0x28: loop start? */
        strwav->sample_rate = read_s32le(0x2c,sf_h);
        /* 0x30: number of 0x800 sectors */
        strwav->flags       = read_u32le(0x34,sf_h);
        strwav->num_samples = read_s32le(0x5c,sf_h);
        strwav->tracks      = read_s32le(0x60,sf_h);

        strwav->loop_start  = 0;
        strwav->loop_end    = 0;

        strwav->codec = PSX;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x8000;
        //todo: tracks are stereo blocks of size 0x20000*tracks, containing 4 interleaves of 0x8000:
        // | 1 2 1 2 | 3 4 3 4 | 5 6 5 6 | 1 2 1 2 | 3 4 3 4 | 5 6 5 6 | ...
        ;VGM_LOG("STR+WAV: header ZPb (PS2)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (PS2)[2002] */
    /* The Fairly OddParents - Breakin' da Rules (PS2)[2003] */
    /* The Fairly OddParents! - Shadow Showdown (PS2)[2004] */
    /* Bad Boys II (PS2)[2004] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 ||   /* BB2 */
         read_u32be(0x04,sf_h) == 0x00000900) &&  /* FOP, ZP */
         read_u32le(0x24,sf_h) == read_u32le(0x70,sf_h) && /* sample rate repeat */
         read_u32le(0x78,sf_h)*0x04 + read_u32le(0x7c,sf_h) == header_size /* ~0xe0 + variable */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x40,sf_h);
        strwav->loop_end    = read_s32le(0x54,sf_h);
        /* 0x70: sample rate 2 */
        /* 0x78: table1 entries */
        /* 0x7c: table1 offset */
        /* 0xb4: number of 0x800 sectors */

        strwav->codec = PSX;
        strwav->interleave  = strwav->tracks > 1 ? 0x4000 : 0x8000;
        ;VGM_LOG("STR+WAV: header FOP/BB2/ZP/PW3 (PS2)\n");
        return 1;
    }

    /* Pac-Man World 3 (PS2)[2005] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 || 
         read_u32be(0x04,sf_h) == 0x01000800) &&  /* rare, mu_spectral1_explore_2 */
         read_u32le(0x24,sf_h) == read_u32le(0x70,sf_h) && /* sample rate repeat */
         read_u32le(0x78,sf_h)*0x04 + read_u32le(0x7c,sf_h) == header_size /* ~0xe0 + variable */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x40,sf_h);
        strwav->loop_end    = read_s32le(0x54,sf_h);
        /* 0x70: sample rate 2 */
        /* 0x78: table1 entries */
        /* 0x7c: table1 offset */
        /* 0xb4: number of 0x800 sectors */
        /* 0xe0: table2 offset */
        /* 0xe4: table2 entries */

        strwav->codec = PSX;
        strwav->interleave  = strwav->tracks > 1 ? 0x4000 : 0x8000;
        ;VGM_LOG("STR+WAV: header FOP/BB2/ZP/PW3 (PS2)\n");
        return 1;
    }

    /* Taz Wanted (PC)[2002] */
    /* Zapper: One Wicked Cricket! Beta (Xbox)[2002] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x0c,sf_h) != header_size &&
         read_u32le(0x24,sf_h) != 0 &&
         read_u32le(0x24,sf_h) == read_u32le(0x90,sf_h) && /* sample rate repeat */
         (read_u32le(0xa0,sf_h) == header_size ||   /* Zapper */
          read_u32le(0xa0,sf_h) + 0x50 == header_size) /* Taz */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x50,sf_h);
        /* 0x58: number of chunks? */
        /* 0x90: sample rate 2 */
        /* 0xb8: total frames? */

        strwav->loop_end    = strwav->num_samples;

        strwav->codec = XBOX;
        strwav->interleave  = strwav->tracks > 1 ? 0xD800/2 : 0xD800;
        ;VGM_LOG("STR+WAV: header ZPb (Xbox)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (Xbox)[2002] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x0c,sf_h) != header_size &&
         read_u32le(0x24,sf_h) != 0 &&
         read_u32le(0x24,sf_h) == read_u32le(0xb0,sf_h) && /* sample rate repeat */
         read_u32le(0xc0,sf_h) == header_size
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        strwav->tracks      = read_s32le(0x70,sf_h);
        /* 0x78: number of chunks? */
        /* 0xb0: sample rate 2 */
        /* 0xc0: header size*/
        /* 0xd8: total frames? */

        strwav->loop_end    = strwav->num_samples;

        strwav->codec = XBOX;
        strwav->interleave  = strwav->tracks > 1 ? 0xD800/2 : 0xD800;
        ;VGM_LOG("STR+WAV: header ZP (Xbox)\n");
        return 1;
    }

    /* Zapper: One Wicked Cricket! (PC)[2002] */
    if ( read_u32be(0x04,sf_h) == 0x00000900 &&
         read_u32le(0x24,sf_h) == read_u32le(0x114,sf_h) && /* sample rate repeat */
         read_u32le(0x12c,sf_h) == header_size /* ~0x130 */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_end    = read_s32le(0x30,sf_h);
        /* 0x38: related to samples? */
        /* 0x54: number of chunks */
        strwav->loop_start  = read_s32le(0x54,sf_h);
        strwav->tracks      = read_s32le(0xF8,sf_h);
        /* 0x114: sample rate 2 */
        /* 0x120: number of blocks */
        /* 0x12c: table1 offset */

        strwav->loop_start = 0; /* ??? */

        strwav->codec = IMA;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;
        ;VGM_LOG("STR+WAV: header ZP (PC)\n");
        return 1;
    }

    /* Pac-Man World 3 (PC)[2005] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 ||
         read_u32be(0x04,sf_h) == 0x01000800) &&  /* rare, mu_spectral1_explore_2 */
         read_u32le(0x24,sf_h) == read_u32le(0x114,sf_h) && /* sample rate repeat */
         read_u32le(0x130,sf_h) + read_u32le(0x134,sf_h) * 0x40 == header_size /* ~0x140 + cues */
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->num_samples = read_s32le(0x20,sf_h);
        strwav->sample_rate = read_s32le(0x24,sf_h);
        /* 0x28: 16 bps */
        strwav->flags       = read_u32le(0x2c,sf_h);
        strwav->loop_end    = read_s32le(0x30,sf_h);
        strwav->loop_start  = read_s32le(0x38,sf_h);
        /* 0x54: number of chunks */
        strwav->tracks      = read_s32le(0xF8,sf_h);
        /* 0x114: sample rate 2 */
        /* 0x120: default hashname? */
        /* 0x124: number of blocks? */
        /* 0x128: table1 entries */
        /* 0x12c: table1 offset */
        /* 0x130: table2 offset */
        /* 0x134: table2 entries */

        strwav->codec = IMA;
        strwav->interleave  = strwav->tracks > 1 ? 0x8000 : 0x10000;
        ;VGM_LOG("STR+WAV: PW3 (PC)\n");
        return 1;
    }

    /* Pac-Man World 3 (GC)[2005] */
    /* SpongeBob SquarePants: Creature from the Krusty Krab (GC)[2006] */
    /* SpongeBob SquarePants: Creature from the Krusty Krab (Wii)[2006] */
    if ( read_u32be(0x04,sf_h) == 0x00000800 &&
         read_u32be(0x24,sf_h) == read_u32be(0xb0,sf_h) && /* sample rate repeat */
         read_u32be(0x24,sf_h) == read_u32be(read_u32be(0xf0,sf_h)+0x08,sf_h) && /* sample rate vs 1st DSP header */
         read_u32be(0xc0,sf_h)*0x04 + read_u32be(0xc4,sf_h) == read_u32be(0xe0,sf_h) && /* main size */
        (read_u32be(0xe0,sf_h) + read_u32be(0xe4,sf_h)*0x40 == header_size || /* main size + extradata 1 (config? PMW3 cs2.wav) */
         read_u32be(0xe0,sf_h) + read_u32be(0xe4,sf_h)*0x08 == header_size) /* main size + extradata 2 (ids? SBSP 0_0_mu_hr.wav) */
         ) {
        strwav->num_samples = read_s32be(0x20,sf_h);
        strwav->sample_rate = read_s32be(0x24,sf_h);
        strwav->flags       = read_u32be(0x2c,sf_h);
        strwav->loop_start  = read_s32be(0xd8,sf_h);
        strwav->loop_end    = read_s32be(0xdc,sf_h);

        strwav->tracks      = read_s32be(0x70,sf_h);

        strwav->codec = DSP;
        strwav->dsps_table = 0xf0;
        strwav->interleave  = strwav->tracks >= 2 ? 0x8000 : 0x10000;
        ;VGM_LOG("STR+WAV: header SBCKK (GC)\n");
        return 1;
    }

    /* SpongeBob SquarePants: Creature from the Krusty Krab (PS2)[2006] */
    /* Sneak King (Xbox)[2006] */
    if ( read_u32be(0x04,sf_h) == 0x00000800 &&
         read_u32le(0x08,sf_h) == 0x00000000 &&
         read_u32le(0x0c,sf_h) != header_size &&
         header_size == 
            read_u32le(0x40,sf_h) + read_u16le(0x48,sf_h) * 0x04 +
            read_u16le(0x4a,sf_h) * ((read_u32le(0x3c,sf_h) & 0x200) ? 0x08+0x38 : 0x08)
         ) {
        /* 0x08: null */
        /* 0x0c: hashname */
        strwav->loop_start  = read_s32le(0x24,sf_h); //ok?
        /* 0x28: f32 time in ms */
        strwav->num_samples = read_s32le(0x30,sf_h);
        strwav->loop_end    = read_s32le(0x34,sf_h);
        strwav->sample_rate = read_s32le(0x38,sf_h);
        strwav->flags       = read_u32le(0x3c,sf_h);
        /* 0x40: table1 offset */
        /* 0x44: table2 offset */
        /* 0x48: table1 entries */
        /* 0x4a: table2 entries */
        /* 0x4c: ? (some low number) */
        strwav->tracks      = read_u8   (0x4e,sf_h);
        /* 0x4f: 16 bps */
        /* 0x54: channels per each track? (ex. 2 stereo track: 0x02,0x02) */
        /* 0x64: channels */
        /* 0x70+: tables */

        /* no codec flags */
        if (ps_check_format(sf_b, 0x00, 0x100)) {
            strwav->codec = PSX;
            strwav->interleave  = strwav->tracks > 2 ? 0x4000 : 0x8000;
        }
        else {
            strwav->codec = XBOX;
            strwav->interleave  = strwav->tracks > 1 ? 0xD800/2 : 0xD800; /* assumed for multitrack */
        }
        ;VGM_LOG("STR+WAV: header SBCKK/SK (PS2)\n");
        return 1;
    }

    /* Tak and the Guardians of Gross (PS2)[2008] */
    /* SpongeBob's Atlantis SquarePantis (PS2)[2007] */
    if ( read_u32be(0x04,sf_h) == 0x00000800 &&
         read_u32le(0x08,sf_h) != 0x00000000 &&     /* some ID */
         read_u32le(0x0c,sf_h) == header_size &&    /* ~0x7c+variable */
         header_size == 
            read_u32le(0x40,sf_h) + read_u16le(0x48,sf_h) * 0x04 +
            read_u16le(0x4a,sf_h) * ((read_u32le(0x3c,sf_h) & 0x200) ? 0x08+0x38 : 0x08)
         ) {
        strwav->loop_start  = read_s32le(0x24,sf_h); //not ok?
        strwav->num_samples = read_s32le(0x30,sf_h);
        strwav->loop_end    = read_s32le(0x34,sf_h);
        strwav->sample_rate = read_s32le(0x38,sf_h);
        strwav->flags       = read_u32le(0x3c,sf_h);

        strwav->channels    = read_s32le(0x70,sf_h); /* tracks of 1ch */

        strwav->codec = PSX;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;
        ;VGM_LOG("STR+WAV: header TKGG/SBASP (PS2)\n");
        return 1;
    }

    /* Tak and the Guardians of Gross (Wii)[2008] */
    /* The House of the Dead: Overkill (Wii)[2009] (not Blitz but still the same format) */
    /* All Star Karate (Wii)[2010] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 ||
         read_u32be(0x04,sf_h) == 0x00000700) && /* rare? */
         read_u32be(0x08,sf_h) != 0x00000000 &&
         read_u32be(0x0c,sf_h) == header_size && /* variable per header */
         read_u32be(0x7c,sf_h) != 0 && /* has DSP header */
         read_u32be(0x38,sf_h) == read_u32be(read_u32be(0x7c,sf_h)+0x38,sf_h) /* sample rate vs 1st DSP header */
         ) {
        strwav->loop_start  = 0; //read_s32be(0x24,sf_h); //not ok?
        strwav->num_samples = read_s32be(0x30,sf_h);
        strwav->loop_end    = read_s32be(0x34,sf_h);
        strwav->sample_rate = read_s32be(0x38,sf_h);
        strwav->flags       = read_u32be(0x3c,sf_h);

        strwav->channels    = read_s32be(0x70,sf_h); /* tracks of 1ch */

        strwav->codec = DSP;
        strwav->coefs_table = 0x7c;
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;
        ;VGM_LOG("STR+WAV: header TKGG/HOTDO/ASK (Wii)\n");
        return 1;
    }

    /* The House of the Dead: Overkill (PS3)[2009] (not Blitz but still the same format) */
    if ((read_u32be(0x04,sf_h) == 0x00000800 ||
         read_u32be(0x04,sf_h) == 0x00000700) && /* rare? */
         read_u32be(0x08,sf_h) != 0x00000000 &&
         read_u32be(0x0c,sf_h) == header_size && /* variable per header */
         read_u32be(0x7c,sf_h) == 0 /* not DSP header */
         ) {
        strwav->loop_start  = 0; //read_32bitLE(0x24,sf_h); //not ok?
        strwav->num_samples = read_s32be(0x30,sf_h);
        strwav->loop_end    = read_s32be(0x34,sf_h);
        strwav->sample_rate = read_s32be(0x38,sf_h);
        strwav->flags       = read_u32be(0x3c,sf_h);

        strwav->channels    = read_s32be(0x70,sf_h); /* tracks of 1ch */
        strwav->interleave  = strwav->channels > 4 ? 0x4000 : 0x8000;

        strwav->codec = PSX;
        ;VGM_LOG("STR+WAV: header HOTDO (PS3)\n");
        return 1;
    }

    /* SpongeBob's Surf & Skate Roadtrip (X360)[2011] */
    if ((read_u32be(0x04,sf_h) == 0x00000800 || /* used? */
         read_u32be(0x04,sf_h) == 0x00000700) &&
         read_u32be(0x08,sf_h) != 0x00000000 &&
         read_u32be(0x0c,sf_h) == 0x124 && /* variable, not sure about final calc */
         read_u32be(0x8c,sf_h) == 0x180 /* encoder delay actually */
         //0x4c is data_size + 0x210
         ) {
        strwav->loop_start  = 0; //read_32bitLE(0x24,sf_h); //not ok?
        strwav->num_samples = read_s32be(0x30,sf_h);//todo sometimes wrong?
        strwav->loop_end    = read_s32be(0x34,sf_h);
        strwav->sample_rate = read_s32be(0x38,sf_h);
        strwav->flags       = read_u32be(0x3c,sf_h);

        strwav->channels    = read_s32be(0x70,sf_h); /* multichannel XMA */

        strwav->codec = XMA2;
        ;VGM_LOG("STR+WAV: header SBSSR (X360)\n");
        return 1;
    }

    /* unknown */
    goto fail;

fail:
    return 0;
}
