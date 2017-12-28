#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"
#include <string.h>

/* RIFF - Resource Interchange File Format, standard container used in many games */

/* return milliseconds */
static long parse_adtl_marker(unsigned char * marker) {
    long hh,mm,ss,ms;
    if (memcmp("Marker ",marker,7)) return -1;

    if (4 != sscanf((char*)marker+7,"%ld:%ld:%ld.%ld",&hh,&mm,&ss,&ms))
        return -1;

    return ((hh*60+mm)*60+ss)*1000+ms;
}

/* loop points have been found hiding here */
static void parse_adtl(off_t adtl_offset, off_t adtl_length, STREAMFILE  *streamFile, long *loop_start, long *loop_end, int *loop_flag) {
    int loop_start_found = 0;
    int loop_end_found = 0;
    off_t current_chunk = adtl_offset+4;

    while (current_chunk < adtl_offset+adtl_length) {
        uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
        off_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

        if (current_chunk+8+chunk_size > adtl_offset+adtl_length) return;

        switch(chunk_type) {
            case 0x6c61626c: {  /* labl */
                unsigned char *labelcontent;
                labelcontent = malloc(chunk_size-4);
                if (!labelcontent) return;
                if (read_streamfile(labelcontent,current_chunk+0xc, chunk_size-4,streamFile)!=chunk_size-4) {
                    free(labelcontent);
                    return;
                }

                switch (read_32bitLE(current_chunk+8,streamFile)) {
                    case 1:
                        if (!loop_start_found && (*loop_start = parse_adtl_marker(labelcontent))>=0)
                            loop_start_found = 1;
                        break;
                    case 2:
                        if (!loop_end_found && (*loop_end = parse_adtl_marker(labelcontent))>=0)
                            loop_end_found = 1;
                        break;
                    default:
                        break;
                }

                free(labelcontent);
                break;
            }
            default:
                break;
        }

        current_chunk += 8 + chunk_size;
    }

    if (loop_start_found && loop_end_found)
        *loop_flag = 1;

    /* labels don't seem to be consistently ordered */
    if (*loop_start > *loop_end) {
        long temp = *loop_start;
        *loop_start = *loop_end;
        *loop_end = temp;
    }
}

typedef struct {
    off_t offset;
    off_t size;
    int sample_rate;
    int channel_count;
    uint32_t block_size;

    int coding_type;
    int interleave;
} riff_fmt_chunk;

static int read_fmt(int big_endian, STREAMFILE * streamFile, off_t current_chunk, riff_fmt_chunk * fmt, int sns, int mwv) {
    int codec, bps;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitBE : read_16bitLE;

    fmt->offset = current_chunk;
    fmt->size = read_32bit(current_chunk+0x4,streamFile);

    fmt->sample_rate = read_32bit(current_chunk+0x0c,streamFile);
    fmt->channel_count = read_16bit(current_chunk+0x0a,streamFile);
    fmt->block_size = read_16bit(current_chunk+0x14,streamFile);
    fmt->interleave = 0;

    bps = read_16bit(current_chunk+0x16,streamFile);
    codec = (uint16_t)read_16bit(current_chunk+0x8,streamFile);

    switch (codec) {
        case 0x01: /* PCM */
            switch (bps) {
                case 16:
                    fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                    fmt->interleave = 2;
                    break;
                case 8:
                    fmt->coding_type = coding_PCM8_U_int;
                    fmt->interleave = 1;
                    break;
                default:
                    goto fail;
            }
            break;

        case 0x02: /* MS ADPCM */
            if (bps != 4) goto fail;
            fmt->coding_type = coding_MSADPCM;
            break;

        case 0x11:  /* MS IMA ADPCM */
            if (bps != 4) goto fail;
            fmt->coding_type = coding_MS_IMA;
            break;

        case 0x69:  /* MS IMA ADPCM (XBOX) - Rayman Raving Rabbids 2 (PC) */
            if (bps != 4) goto fail;
            fmt->coding_type = coding_MS_IMA;
            break;

        case 0x007A:  /* MS IMA ADPCM (LA Rush, Psi Ops PC) */
            /* 0x007A is apparently "Voxware SC3" but in .MED it's just MS-IMA */
            if (!check_extensions(streamFile,"med"))
                goto fail;

            if (bps == 4) /* normal MS IMA */
                fmt->coding_type = coding_MS_IMA;
            else if (bps == 3) /* 3-bit MS IMA, used in a very few files */
                goto fail; //fmt->coding_type = coding_MS_IMA_3BIT;
            else
                goto fail;
            break;

        case 0x0555: /* Level-5 0x555 ADPCM */
            if (!mwv) goto fail;
            fmt->coding_type = coding_L5_555;
            fmt->interleave = 0x12;
            break;

        case 0x5050: /* Ubisoft .sns uses this for DSP */
            if (!sns) goto fail;
            fmt->coding_type = coding_NGC_DSP;
            fmt->interleave = 0x08;
            break;

        case 0x270: /* ATRAC3 */
#ifdef VGM_USE_FFMPEG
            fmt->coding_type = coding_FFmpeg;
            break;
#else
            goto fail;
#endif

        case 0xFFFE: /* WAVEFORMATEXTENSIBLE */

            /* ATRAC3plus GUID (0xBFAA23E9 58CB7144 A119FFFA 01E4CE62) */
            if (read_32bit(current_chunk+0x20,streamFile) == 0xE923AABF &&
                read_16bit(current_chunk+0x24,streamFile) == (int16_t)0xCB58 &&
                read_16bit(current_chunk+0x26,streamFile) == 0x4471 &&
                read_32bitLE(current_chunk+0x28,streamFile) == 0xFAFF19A1 &&
                read_32bitLE(current_chunk+0x2C,streamFile) == 0x62CEE401) {
#ifdef VGM_USE_MAIATRAC3PLUS
                uint16_t bztmp = read_16bit(current_chunk+0x32,streamFile);
                bztmp = (bztmp >> 8) | (bztmp << 8);
                fmt->coding_type = coding_AT3plus;
                fmt->block_size = (bztmp & 0x3FF) * 8 + 8; //should match fmt->block_size
#elif defined(VGM_USE_FFMPEG)
                fmt->coding_type = coding_FFmpeg;
#else
                goto fail;
#endif
            }

            /* ATRAC9 GUID 47E142D2-36BA-4d8d-88FC-61654F8C836C (D242E147 BA368D4D 88FC6165 4F8C836C) */
            if (read_32bitBE(current_chunk+0x20,streamFile) == 0xD242E147 &&
                read_32bitBE(current_chunk+0x24,streamFile) == 0xBA368D4D &&
                read_32bitBE(current_chunk+0x28,streamFile) == 0x88FC6165 &&
                read_32bitBE(current_chunk+0x2c,streamFile) == 0x4F8C836C) {
#ifdef VGM_USE_ATRAC9
                fmt->coding_type = coding_ATRAC9;
#else
                goto fail;
#endif
            }

            break;

        default:
            goto fail;
    }

    return 0;

fail:
    return -1;
}

VGMSTREAM * init_vgmstream_riff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    riff_fmt_chunk fmt = {0};

    size_t file_size, riff_size, data_size = 0;
    off_t start_offset = 0;

    int fact_sample_count = -1;
    int fact_sample_skip = -1;

    int loop_flag = 0;
    long loop_start_ms = -1;
    long loop_end_ms = -1;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;

    int FormatChunkFound = 0, DataChunkFound = 0, JunkFound = 0;

    int mwv = 0; /* Level-5 .mwv (Dragon Quest VIII, Rogue Galaxy) */
    off_t mwv_pflt_offset = -1;
    off_t mwv_ctrl_offset = -1;
    int sns = 0; /* Ubisoft .sns LyN engine (Red Steel 2, Just Dance 3) */
    int at3 = 0; /* Sony ATRAC3 / ATRAC3plus */
    int at9 = 0; /* Sony ATRAC9 */

    /* check extension, case insensitive
     * .da: The Great Battle VI (PS), .cd: Exector (PS), .med: Psi Ops (PC) */
    if ( check_extensions(streamFile, "wav,lwav,da,cd,med") ) {
        ;
    }
    else if ( check_extensions(streamFile, "mwv") ) {
        mwv = 1;
    }
    else if ( check_extensions(streamFile, "sns") ) {
        sns = 1;
    }
    /* .rws: Climax games (Silent Hill Origins PSP, Oblivion PSP), .aud: EA Replay */
    else if ( check_extensions(streamFile, "at3,rws,aud") ) {
        at3 = 1;
    }
    else if ( check_extensions(streamFile, "at9") ) {
        at9 = 1;
    }
    else {
        goto fail;
    }

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x57415645) /* "WAVE" */
        goto fail;

    riff_size = read_32bitLE(0x04,streamFile);
    file_size = get_streamfile_size(streamFile);

    /* check for truncated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (-1 == read_fmt(0, /* big endian == false*/
                        streamFile,
                        current_chunk,
                        &fmt,
                        sns,
                        mwv))
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x4C495354:    /* LIST */
                    /* what lurks within?? */
                    switch (read_32bitBE(current_chunk + 8, streamFile)) {
                        case 0x6164746C:    /* adtl */
                            /* yay, atdl is its own little world */
                            parse_adtl(current_chunk + 8, chunk_size,
                                    streamFile,
                                    &loop_start_ms,&loop_end_ms,&loop_flag);
                            break;
                        default:
                            break;
                    }
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count and loop info */
                    if (read_32bitLE(current_chunk+0x24, streamFile)==1) {
                        if (read_32bitLE(current_chunk+0x2c+4, streamFile)==0) {
                            loop_flag = 1;
                            loop_start_offset = read_32bitLE(current_chunk+0x2c+8, streamFile);
                            loop_end_offset = read_32bitLE(current_chunk+0x2c+0xc,streamFile);
                        }
                    }
                    break;
                case 0x70666c74:    /* pflt */
                    if (!mwv) break;    /* ignore if not in an mwv */

                    mwv_pflt_offset = current_chunk; /* predictor filters */
                    break;
                case 0x6374726c:    /* ctrl */
                    if (!mwv) break;

                    loop_flag = read_32bitLE(current_chunk+0x08, streamFile);
                    mwv_ctrl_offset = current_chunk;
                    break;
                case 0x66616374:    /* fact */
                    if (sns && chunk_size == 0x10) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                    } else if ((at3 || at9) && chunk_size == 0x08) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x0c, streamFile);
                    } else if ((at3 || at9) && chunk_size == 0x0c) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x10, streamFile);
                    }

                    break;
                case 0x4A554E4B:    /* JUNK */
                    JunkFound = 1;
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    /* JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
     * To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
     * As JUNK is legal (if unusual) we only reject those codecs.
     * (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (JunkFound
            && check_extensions(streamFile,"wav,lwav") /* for some .MED IMA */
            && (fmt.coding_type==coding_MSADPCM || fmt.coding_type==coding_MS_IMA))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fmt.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fmt.sample_rate;

    /* init, samples */
    switch (fmt.coding_type) {
        case coding_PCM16LE:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, fmt.channel_count, 16);
            break;
        case coding_PCM8_U_int:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, vgmstream->channels, 8);
            break;
        case coding_L5_555:
            vgmstream->num_samples = data_size / 0x12 / fmt.channel_count * 32;

            /* coefs */
            if (mwv) {
                int i, ch;
                const int filter_order = 3;
                int filter_count = read_32bitLE(mwv_pflt_offset+0x0c, streamFile);
                if (filter_count > 0x20) goto fail;

                if (mwv_pflt_offset == -1 ||
                        read_32bitLE(mwv_pflt_offset+0x08, streamFile) != filter_order ||
                        read_32bitLE(mwv_pflt_offset+0x04, streamFile) < 8 + filter_count * 4 * filter_order)
                    goto fail;

                for (ch = 0; ch < fmt.channel_count; ch++) {
                    for (i = 0; i < filter_count * filter_order; i++) {
                        int coef = read_32bitLE(mwv_pflt_offset+0x10+i*0x04, streamFile);
                        vgmstream->ch[ch].adpcm_coef_3by32[i] = coef;
                    }
                }
            }

            break;
        case coding_MSADPCM:
            vgmstream->num_samples = msadpcm_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            break;
        case coding_MS_IMA:
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            break;
        case coding_NGC_DSP:
            //sample_count = dsp_bytes_to_samples(data_size, fmt.channel_count); /* expected from the "fact" chunk */

            /* coefs */
            if (sns) {
                int i, ch;
                static const int16_t coef[16] = { /* common codebook? */
                        0x04ab,0xfced,0x0789,0xfedf,0x09a2,0xfae5,0x0c90,0xfac1,
                        0x084d,0xfaa4,0x0982,0xfdf7,0x0af6,0xfafa,0x0be6,0xfbf5
                };

                for (ch = 0; ch < fmt.channel_count; ch++) {
                    for (i = 0; i < 16; i++) {
                        vgmstream->ch[ch].adpcm_coef[i] = coef[i];
                    }
                }
            }

            break;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *ffmpeg_data = init_ffmpeg_offset(streamFile, 0x00, streamFile->get_size(streamFile));
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;

            vgmstream->num_samples = ffmpeg_data->totalSamples; /* fact_sample_count */

            if (at3) {
                /* the encoder introduces some garbage (not always silent) samples to skip before the stream */
                /* manually set skip_samples if FFmpeg didn't do it */
                if (ffmpeg_data->skipSamples <= 0) {
                    ffmpeg_set_skip_samples(ffmpeg_data, fact_sample_skip);
                }

                /* RIFF loop/sample values are absolute (with skip samples), adjust */
                if (loop_flag) {
                    loop_start_offset -= ffmpeg_data->skipSamples;
                    loop_end_offset -= ffmpeg_data->skipSamples;
                }
            }
            break;
        }
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus: {
            vgmstream->codec_data = init_at3plus();

            /* get rough total samples but favor fact_samples if available (skip isn't correctly handled for now) */
            vgmstream->num_samples = atrac3plus_bytes_to_samples(data_size, fmt.block_size);
            if (fact_sample_count > 0 && fact_sample_count + fact_sample_skip < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count + fact_sample_skip;
            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9: {
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(fmt.offset+0x08+0x2c,streamFile);
            cfg.encoder_delay = fact_sample_skip;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = fact_sample_count;
            /* RIFF loop/sample values are absolute (with skip samples), adjust */
            if (loop_flag) {
                loop_start_offset -= fact_sample_skip;
                loop_end_offset -= fact_sample_skip;
            }

            break;
        }
#endif
        default:
            goto fail;
    }
    /* .sns uses fact chunk */
    if (sns) {
        if (-1 == fact_sample_count) goto fail;
        vgmstream->num_samples = fact_sample_count;
    }

    /* coding, layout, interleave */
    vgmstream->coding_type = fmt.coding_type;
    switch (fmt.coding_type) {
        case coding_MSADPCM:
        case coding_MS_IMA:
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
#endif
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = fmt.block_size;
            break;
        default:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = fmt.interleave;
            break;
    }

    /* meta, loops */
    vgmstream->meta_type = meta_RIFF_WAVE;
    if (loop_flag) {
        if (loop_start_ms >= 0) {
            vgmstream->loop_start_sample = (long long)loop_start_ms*fmt.sample_rate/1000;
            vgmstream->loop_end_sample = (long long)loop_end_ms*fmt.sample_rate/1000;
            vgmstream->meta_type = meta_RIFF_WAVE_labl;
        }
        else if (loop_start_offset >= 0) {
            vgmstream->loop_start_sample = loop_start_offset;
            vgmstream->loop_end_sample = loop_end_offset;
            vgmstream->meta_type = meta_RIFF_WAVE_smpl;
        }
        else if (mwv && mwv_ctrl_offset != -1) {
            vgmstream->loop_start_sample = read_32bitLE(mwv_ctrl_offset+12, streamFile);
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }
    if (mwv) {
        vgmstream->meta_type = meta_RIFF_WAVE_MWV;
    }
    if (sns) {
        vgmstream->meta_type = meta_RIFF_WAVE_SNS;
    }


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;


fail:
    close_vgmstream(vgmstream);
    return NULL;
}

VGMSTREAM * init_vgmstream_rifx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    riff_fmt_chunk fmt = {0};

    size_t file_size, riff_size, data_size = 0;
    off_t start_offset = 0;

    int loop_flag = 0;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;

    int FormatChunkFound = 0, DataChunkFound = 0;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "wav,lwav") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494658) /* "RIFX" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x57415645) /* "WAVE" */
        goto fail;

    riff_size = read_32bitBE(0x04,streamFile);
    file_size = get_streamfile_size(streamFile);

    /* check for truncated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitBE(current_chunk+4,streamFile);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (-1 == read_fmt(1, /* big endian == true */
                        streamFile,
                        current_chunk,
                        &fmt,
                        0,  /* sns == false */
                        0)) /* mwv == false */
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count and loop info */
                    if (read_32bitBE(current_chunk+0x24, streamFile)==1) {
                        if (read_32bitBE(current_chunk+0x2c+4, streamFile)==0) {
                            loop_flag = 1;
                            loop_start_offset = read_32bitBE(current_chunk+0x2c+8, streamFile);
                            loop_end_offset = read_32bitBE(current_chunk+0x2c+0xc,streamFile);
                        }
                    }
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fmt.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fmt.sample_rate;

    /* init, samples */
    switch (fmt.coding_type) {
        case coding_PCM16BE:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, vgmstream->channels, 16);
            break;
        case coding_PCM8_U_int:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, vgmstream->channels, 8);
            break;
        default:
            goto fail;
    }

    /* coding, layout, interleave */
    vgmstream->coding_type = fmt.coding_type;
    switch (fmt.coding_type) {
        default:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = fmt.interleave;
            break;
    }

    /* meta, loops */
    vgmstream->meta_type = meta_RIFX_WAVE;
    if (loop_flag) {
        if (loop_start_offset >= 0) {
            vgmstream->loop_start_sample = loop_start_offset;
            vgmstream->loop_end_sample = loop_end_offset;
            vgmstream->meta_type = meta_RIFX_WAVE_smpl;
        }
    }


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
