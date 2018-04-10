#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"
#include <string.h>

/* RIFF - Resource Interchange File Format, standard container used in many games */

#ifdef VGM_USE_VORBIS
static VGMSTREAM *parse_riff_ogg(STREAMFILE * streamFile, off_t start_offset, size_t data_size);
#endif

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
    uint32_t codec;
    int sample_rate;
    int channel_count;
    uint32_t block_size;
    int bps;

    int coding_type;
    int interleave;
} riff_fmt_chunk;

static int read_fmt(int big_endian, STREAMFILE * streamFile, off_t current_chunk, riff_fmt_chunk * fmt, int mwv) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitBE : read_16bitLE;

    fmt->offset = current_chunk;
    fmt->size = read_32bit(current_chunk+0x4,streamFile);

    fmt->sample_rate = read_32bit(current_chunk+0x0c,streamFile);
    fmt->channel_count = read_16bit(current_chunk+0x0a,streamFile);
    fmt->block_size = read_16bit(current_chunk+0x14,streamFile);
    fmt->interleave = 0;

    fmt->bps = read_16bit(current_chunk+0x16,streamFile);
    fmt->codec = (uint16_t)read_16bit(current_chunk+0x8,streamFile);

    switch (fmt->codec) {
        case 0x00:  /* Yamaha ADPCM (raw) [Headhunter (DC), Bomber hehhe (DC)] (unofficial) */
            if (fmt->bps != 4) goto fail;
            if (fmt->block_size != 0x02*fmt->channel_count) goto fail;
            fmt->coding_type = coding_AICA_int;
            fmt->interleave = 0x01;
            break;

        case 0x01: /* PCM */
            switch (fmt->bps) {
                case 16:
                    fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                    fmt->interleave = 0x02;
                    break;
                case 8:
                    fmt->coding_type = coding_PCM8_U_int;
                    fmt->interleave = 0x01;
                    break;
                default:
                    goto fail;
            }
            break;

        case 0x02: /* MS ADPCM */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_MSADPCM;
            break;

        case 0x11:  /* MS IMA ADPCM [Layton Brothers: Mystery Room (iOS/Android)] */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_MS_IMA;
            break;

        case 0x20:  /* Yamaha ADPCM (raw) [Takuyo/Dynamix/etc DC games] */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_AICA;
            break;

        case 0x69:  /* XBOX IMA ADPCM [Dynasty Warriors 5 (Xbox)] */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_XBOX_IMA;
            break;

        case 0x007A:  /* MS IMA ADPCM [LA Rush (PC), Psi Ops (PC)] (unofficial) */
            /* 0x007A is apparently "Voxware SC3" but in .MED it's just MS-IMA (0x11) */
            if (!check_extensions(streamFile,"med"))
                goto fail;

            if (fmt->bps == 4) /* normal MS IMA */
                fmt->coding_type = coding_MS_IMA;
            else if (fmt->bps == 3) /* 3-bit MS IMA, used in a very few files */
                goto fail; //fmt->coding_type = coding_MS_IMA_3BIT;
            else
                goto fail;
            break;

        case 0x0555: /* Level-5 0x555 ADPCM (unofficial) */
            if (!mwv) goto fail;
            fmt->coding_type = coding_L5_555;
            fmt->interleave = 0x12;
            break;

#ifdef VGM_USE_VORBIS
        case 0x6771: /* Ogg Vorbis (mode 3+) */
            fmt->coding_type = coding_OGG_VORBIS;
            break;
#else
            goto fail;
#endif

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
                break;
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
                break;
#else
                goto fail;
#endif
            }

            goto fail; /* unknown GUID */

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

    int fact_sample_count = 0;
    int fact_sample_skip = 0;

    int loop_flag = 0;
    long loop_start_ms = -1, loop_end_ms = -1;
    int32_t loop_start_wsmp = -1, loop_end_wsmp = -1;
    int32_t loop_start_smpl = -1, loop_end_smpl = -1;

    int FormatChunkFound = 0, DataChunkFound = 0, JunkFound = 0;

    int mwv = 0; /* Level-5 .mwv (Dragon Quest VIII, Rogue Galaxy) */
    off_t mwv_pflt_offset = -1;
    off_t mwv_ctrl_offset = -1;
    int at3 = 0; /* Sony ATRAC3 / ATRAC3plus */
    int at9 = 0; /* Sony ATRAC9 */


    /* check extension */
    /* .lwav: to avoid hijacking .wav, .xwav: fake for Xbox games (unneded anymore) */
    /* .da: The Great Battle VI (PS), .cd: Exector (PS), .med: Psi Ops (PC), .snd: Layton Brothers (iOS/Android),
     * .adx: Remember11 (PC) sfx
     * .adp: Headhunter (DC) */
    if ( check_extensions(streamFile, "wav,lwav,xwav,da,cd,med,snd,adx,adp") ) {
        ;
    }
    else if ( check_extensions(streamFile, "mwv") ) {
        mwv = 1;
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

    /* for some of Liar-soft's buggy RIFF+Ogg made with Soundforge [Shikkoku no Sharnoth (PC)] */
    if (riff_size+0x08+0x01 == file_size)
        riff_size += 0x01;

    /* some Xbox games do this [Dynasty Warriors 3 (Xbox), BloodRayne (Xbox)] */
    if (riff_size == file_size && read_16bitLE(0x14,streamFile)==0x0069)
        riff_size -= 0x08;
    /* some Dreamcast/Naomi games do this [Headhunter (DC), Bomber hehhe (DC)] */
    if (riff_size + 0x04 == file_size && read_16bitLE(0x14,streamFile)==0x0000)
        riff_size -= 0x04;


    /* check for truncated RIFF */
    if (file_size < riff_size+0x08) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            size_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

            if (fmt.codec == 0x6771 && chunk_type == 0x64617461) /* Liar-soft again */
                chunk_size += (chunk_size%2) ? 0x01 : 0x00;

            if (current_chunk+0x08+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    if (FormatChunkFound) goto fail; /* only one per file */
                    FormatChunkFound = 1;

                    if (-1 == read_fmt(0, /* big endian == false*/
                        streamFile,
                        current_chunk,
                        &fmt,
                        mwv))
                        goto fail;

                    /* some Dreamcast/Naomi games again [Headhunter (DC), Bomber hehhe (DC)] */
                    if (fmt.codec == 0x0000 && chunk_size == 0x12)
                        chunk_size += 0x02;
                    break;

                case 0x64617461:    /* "data" */
                    if (DataChunkFound) goto fail; /* only one per file */
                    DataChunkFound = 1;

                    start_offset = current_chunk + 0x08;
                    data_size = chunk_size;
                    break;

                case 0x4C495354:    /* "LIST" */
                    /* what lurks within?? */
                    switch (read_32bitBE(current_chunk+0x08, streamFile)) {
                        case 0x6164746C:    /* "adtl" */
                            /* yay, atdl is its own little world */
                            parse_adtl(current_chunk + 8, chunk_size,
                                    streamFile,
                                    &loop_start_ms,&loop_end_ms,&loop_flag);
                            break;
                        default:
                            break;
                    }
                    break;

                case 0x736D706C:    /* "smpl" (RIFFMIDISample + MIDILoop chunk) */
                    /* check loop count/loop info (most common) *///todo double check values
                    /* 0x00: manufacturer id, 0x04: product id, 0x08: sample period, 0x0c: unity node,
                     * 0x10: pitch fraction, 0x14: SMPTE format, 0x18: SMPTE offset, 0x1c: loop count, 0x20: sampler data */
                    if (read_32bitLE(current_chunk+0x08+0x1c, streamFile)==1) {
                        /* 0x24: cue point id, 0x28: type (0=forward, 1=alternating, 2=backward)
                         * 0x2c: start, 0x30: end, 0x34: fraction, 0x38: play count */
                        if (read_32bitLE(current_chunk+0x08+0x28, streamFile)==0) {
                            loop_flag = 1;
                            loop_start_smpl = read_32bitLE(current_chunk+0x08+0x2c, streamFile);
                            loop_end_smpl   = read_32bitLE(current_chunk+0x08+0x30, streamFile);
                        }
                    }
                    break;

                case 0x77736D70:    /* "wsmp" (RIFFDLSSample + DLSLoop chunk)  */
                    /* check loop count/info (found in some Xbox games: Halo (non-looping), Dynasty Warriors 3, Crimson Sea) */
                    /* 0x00: size, 0x04: unity note, 0x06: fine tune, 0x08: gain, 0x10: loop count */
                    if (chunk_size >= 0x24
                            && read_32bitLE(current_chunk+0x08+0x00, streamFile) == 0x14
                            && read_32bitLE(current_chunk+0x08+0x10, streamFile) > 0
                            && read_32bitLE(current_chunk+0x08+0x14, streamFile) == 0x10) {
                        /* 0x14: size, 0x18: loop type (0=forward, 1=release), 0x1c: loop start, 0x20: loop length */
                        if (read_32bitLE(current_chunk+0x08+0x18, streamFile)==0) {
                            loop_flag = 1;
                            loop_start_wsmp = read_32bitLE(current_chunk+0x08+0x1c, streamFile);
                            loop_end_wsmp   = read_32bitLE(current_chunk+0x08+0x20, streamFile);
                            loop_end_wsmp  += loop_start_wsmp;
                        }
                    }
                    break;

                case 0x66616374:    /* "fact" */
                    if (chunk_size == 0x04) { /* standard, usually found with ADPCM */
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                    } else if (chunk_size == 0x10 && read_32bitBE(current_chunk+0x08+0x04, streamFile) == 0x4C794E20) { /* "LyN " */
                        goto fail; /* parsed elsewhere */
                    } else if ((at3 || at9) && chunk_size == 0x08) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x0c, streamFile);
                    } else if ((at3 || at9) && chunk_size == 0x0c) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, streamFile);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x10, streamFile);
                    }
                    break;

                case 0x70666c74:    /* "pflt" (.mwv extension) */
                    if (!mwv) break;    /* ignore if not in an mwv */
                    mwv_pflt_offset = current_chunk; /* predictor filters */
                    break;
                case 0x6374726c:    /* "ctrl" (.mwv extension) */
                    if (!mwv) break;
                    loop_flag = read_32bitLE(current_chunk+0x08, streamFile);
                    mwv_ctrl_offset = current_chunk;
                    break;

                case 0x4A554E4B:    /* "JUNK" */
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

    //todo improve detection using fmt sizes/values as Wwise's don't match the RIFF standard
    /* JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
     * To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
     * As JUNK is legal (if unusual) we only reject those codecs.
     * (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (JunkFound
            && check_extensions(streamFile,"wav,lwav") /* for some .MED IMA */
            && (fmt.coding_type==coding_MSADPCM /*|| fmt.coding_type==coding_MS_IMA*/ || fmt.coding_type==coding_XBOX_IMA))
        goto fail;

    /* ignore Beyond Good & Evil HD PS3 evil reuse of PCM codec */
    if (fmt.coding_type == coding_PCM16LE &&
            read_32bitBE(start_offset+0x00, streamFile) == 0x4D534643 && /* "MSF\43" */
            read_32bitBE(start_offset+0x34, streamFile) == 0xFFFFFFFF && /* always */
            read_32bitBE(start_offset+0x38, streamFile) == 0xFFFFFFFF &&
            read_32bitBE(start_offset+0x3c, streamFile) == 0xFFFFFFFF)
        goto fail;


#ifdef VGM_USE_VORBIS
    /* special case using init_vgmstream_ogg_vorbis */
    if (fmt.coding_type == coding_OGG_VORBIS) {
        return parse_riff_ogg(streamFile, start_offset, data_size);
    }
#endif


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
            if (!mwv) goto fail;
            vgmstream->num_samples = data_size / 0x12 / fmt.channel_count * 32;

            /* coefs */
            {
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
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count;
            break;

        case coding_MS_IMA:
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, fmt.block_size, fmt.channel_count);
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count;
            break;

        case coding_AICA:
        case coding_AICA_int:
            vgmstream->num_samples = aica_bytes_to_samples(data_size, fmt.channel_count);
            break;

        case coding_XBOX_IMA:
            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, fmt.channel_count);
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count; /* some (converted?) Xbox games have bigger fact_samples */
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
                    loop_start_smpl -= (int32_t)ffmpeg_data->skipSamples;
                    loop_end_smpl   -= (int32_t)ffmpeg_data->skipSamples;
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
                loop_start_smpl -= fact_sample_skip;
                loop_end_smpl   -= fact_sample_skip;
            }

            break;
        }
#endif
        default:
            goto fail;
    }

    /* coding, layout, interleave */
    vgmstream->coding_type = fmt.coding_type;
    switch (fmt.coding_type) {
        case coding_MSADPCM:
        case coding_MS_IMA:
        case coding_AICA:
        case coding_XBOX_IMA:
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
        else if (loop_start_smpl >= 0) {
            vgmstream->loop_start_sample = loop_start_smpl;
            vgmstream->loop_end_sample = loop_end_smpl;
            vgmstream->meta_type = meta_RIFF_WAVE_smpl;
        }
        else if (loop_start_wsmp >= 0) {
            vgmstream->loop_start_sample = loop_start_wsmp;
            vgmstream->loop_end_sample = loop_end_wsmp;
            vgmstream->meta_type = meta_RIFF_WAVE_wsmp;
        }
        else if (mwv && mwv_ctrl_offset != -1) {
            vgmstream->loop_start_sample = read_32bitLE(mwv_ctrl_offset+12, streamFile);
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }
    if (mwv) {
        vgmstream->meta_type = meta_RIFF_WAVE_MWV;
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


#ifdef VGM_USE_VORBIS
typedef struct {
    off_t patch_offset;
} riff_ogg_io_data;

static size_t riff_ogg_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, riff_ogg_io_data* data) {
    size_t bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* has garbage init Oggs pages, patch bad flag */
    if (data->patch_offset && data->patch_offset >= offset && data->patch_offset < offset + bytes_read) {
        VGM_ASSERT(dest[data->patch_offset - offset] != 0x02, "RIFF Ogg: bad patch offset\n");
        dest[data->patch_offset - offset] = 0x00;
    }

    return bytes_read;
}

/* special handling of Liar-soft's buggy RIFF+Ogg made with Soundforge [Shikkoku no Sharnoth (PC)] */
static VGMSTREAM *parse_riff_ogg(STREAMFILE * streamFile, off_t start_offset, size_t data_size) {
    off_t patch_offset = 0;
    size_t real_size = data_size;

    /* initial page flag is repeated and causes glitches in decoders, find bad offset */
    {
        off_t offset = start_offset + 0x04+0x02;
        off_t offset_limit = start_offset + data_size; /* usually in the first 0x3000 but can be +0x100000 */

        while (offset < offset_limit) {
            if (read_32bitBE(offset+0x00, streamFile) == 0x4f676753 &&  /* "OggS" */
                read_16bitBE(offset+0x04, streamFile) == 0x0002) {      /* start page flag */

                //todo callback should patch on-the-fly by analyzing all "OggS", but is problematic due to arbitrary offsets
                if (patch_offset) {
                    VGM_LOG("RIFF Ogg: found multiple repeated start pages\n");
                    return NULL;
                }

                patch_offset = offset /*- start_offset*/ + 0x04+0x01;
            }
            offset++; //todo could be optimized to do OggS page sizes
        }
    }

    /* last pages don't have the proper flag and confuse decoders, find actual end */
    {
        size_t max_size = data_size;
        off_t offset_limit = start_offset + data_size - 0x1000; /* not worth checking more, let decoder try */
        off_t offset = start_offset + data_size - 0x1a;

        while (offset > offset_limit) {
            if (read_32bitBE(offset+0x00, streamFile) == 0x4f676753) { /* "OggS" */
                if (read_16bitBE(offset+0x04, streamFile) == 0x0004) { /* last page flag */
                    real_size = max_size;
                    break;
                } else {
                    max_size = offset - start_offset; /* ignore bad pages */
                }
            }
            offset--;
        }
    }

    /* Soundforge includes fact_samples but should be equal to Ogg samples */

    /* actual Ogg init with custom callback to patch weirdness */
    {
        VGMSTREAM *vgmstream = NULL;
        STREAMFILE *custom_streamFile = NULL;
        ogg_vorbis_meta_info_t ovmi = {0};
        riff_ogg_io_data io_data = {0};
        size_t io_data_size = sizeof(riff_ogg_io_data);


        ovmi.meta_type = meta_RIFF_WAVE;
        ovmi.stream_size = real_size;
        //inf.loop_flag = 0; /* not observed */

        io_data.patch_offset = patch_offset;

        custom_streamFile = open_io_streamfile(open_wrap_streamfile(streamFile), &io_data,io_data_size, riff_ogg_io_read,NULL);
        if (!custom_streamFile) return NULL;

        vgmstream = init_vgmstream_ogg_vorbis_callbacks(custom_streamFile, NULL, start_offset, &ovmi);

        close_streamfile(custom_streamFile);

        return vgmstream;
    }
}
#endif
