#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "../util/endianness.h"


/* Wwise uses a custom RIFF/RIFX header, non-standard enough that it's parsed it here.
 * There is some repetition from other metas, but not enough to bother me.
 *
 * Some info: https://www.audiokinetic.com/en/library/edge/
 * .bnk (dynamic music/loop) info: https://github.com/bnnm/wwiser
 */
typedef enum { PCM, IMA, VORBIS, DSP, XMA2, XWMA, AAC, HEVAG, ATRAC9, OPUSNX, OPUS, OPUSCPR, OPUSWW, PTADPCM } wwise_codec;
typedef struct {
    int big_endian;
    size_t file_size;
    int prefetch;
    int is_wem;
    int is_bnk;

    /* chunks references */
    off_t  fmt_offset;
    size_t fmt_size;
    off_t  data_offset;
    size_t data_size;
    off_t  xma2_offset;
    size_t xma2_size;
    off_t  vorb_offset;
    size_t vorb_size;
    off_t  wiih_offset;
    size_t wiih_size;
    off_t  smpl_offset;
    size_t smpl_size;
    off_t  seek_offset;
    size_t seek_size;
    off_t  meta_offset;
    size_t meta_size;

    /* standard fmt stuff */
    wwise_codec codec;
    int format;
    int channels;
    int sample_rate;
    int block_size;
    int avg_bitrate;
    int bits_per_sample;
    uint8_t channel_type;
    uint32_t channel_layout;
    size_t extra_size;

    int32_t num_samples;
    int loop_flag;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
} wwise_header;

static int parse_wwise(STREAMFILE* sf, wwise_header* ww);
static int is_dsp_full_interleave(STREAMFILE* sf, wwise_header* ww, off_t coef_offset, int full_detection);

/* Wwise - Audiokinetic Wwise (WaveWorks Interactive Sound Engine) middleware */
VGMSTREAM* init_vgmstream_wwise(STREAMFILE* sf) {
    return init_vgmstream_wwise_bnk(sf, NULL);
}

/* used in .bnk */
VGMSTREAM* init_vgmstream_wwise_bnk(STREAMFILE* sf, int* p_prefetch) {
    VGMSTREAM* vgmstream = NULL;
    wwise_header ww = {0};
    off_t start_offset;
    read_u32_t read_u32 = NULL;
    read_s32_t read_s32 = NULL;
    read_u16_t read_u16 = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "RIFF") &&  /* LE */
        !is_id32be(0x00,sf, "RIFX"))    /* BE */
        goto fail;

    /* note that Wwise allows those extensions only, so custom engine exts shouldn't be added
     * .wem: newer "Wwise Encoded Media" used after the 2011.2 SDK (~july 2011)
     * .wav: older PCM/ADPCM files [Spider-Man: Web of Shadows (PC), Punch Out!! (Wii)]
     * .xma: older XMA files [Too Human (X360), Tron Evolution (X360)]
     * .ogg: older Vorbis files [The King of Fighters XII (X360)]
     * .bnk: Wwise banks for memory .wem detection (hack) */
    if (!check_extensions(sf,"wem,wav,lwav,ogg,logg,xma,bnk"))
        goto fail;

    ww.is_bnk = (p_prefetch != NULL);
    if (!parse_wwise(sf, &ww))
        goto fail;

    if (p_prefetch)
        *p_prefetch = ww.prefetch;

    read_u32 = ww.big_endian ? read_u32be : read_u32le;
    read_s32 = ww.big_endian ? read_s32be : read_s32le;
    read_u16 = ww.big_endian ? read_u16be : read_u16le;

    start_offset = ww.data_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ww.channels, ww.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WWISE_RIFF;
    vgmstream->sample_rate = ww.sample_rate;
    vgmstream->loop_start_sample = ww.loop_start_sample;
    vgmstream->loop_end_sample = ww.loop_end_sample;
    vgmstream->channel_layout = ww.channel_layout;
    vgmstream->stream_size = ww.data_size;

    switch(ww.codec) {
        case PCM: /* common */
            /* normally riff.c has priority but it's needed when .wem is used */
            /* old=0x10, 0x12=Army of Two: the 40th Day (PS3), new/Limbo (PC) */
            if (ww.fmt_size != 0x10 && ww.fmt_size != 0x12 && ww.fmt_size != 0x18 && ww.fmt_size != 0x28) goto fail;
            if (ww.bits_per_sample != 16) goto fail;

            vgmstream->coding_type = (ww.big_endian ? coding_PCM16BE : coding_PCM16LE);
            vgmstream->layout_type = ww.channels > 1 ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = 0x02;

            if (ww.prefetch) {
                ww.data_size = ww.file_size - ww.data_offset;
            }

            vgmstream->num_samples = pcm_bytes_to_samples(ww.data_size, ww.channels, ww.bits_per_sample);

            /* prefetch .bnk RIFFs that only have header and no data is possible [Metal Gear Solid V (PC)] */
            if (ww.prefetch && !vgmstream->num_samples)
                vgmstream->num_samples = 1; /* force something to avoid broken subsongs */
            break;

        case IMA: /* common */
            /* slightly modified and mono-interleaved XBOX-IMA */
            /* Wwise reuses common codec ids (ex. 0x0002 MSADPCM) for IMA so this parser should go AFTER riff.c avoid misdetection */

            if (ww.fmt_size != 0x14 && ww.fmt_size != 0x28 && ww.fmt_size != 0x18) goto fail; /* oldest, old, new */
            if (ww.bits_per_sample != 4) goto fail;
            if (ww.block_size != 0x24 * ww.channels) goto fail;

            vgmstream->coding_type = coding_WWISE_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = ww.block_size / ww.channels;
            vgmstream->codec_endian = ww.big_endian;

            /* oldest version uses regular XBOX IMA with stereo mode [Shadowrun (PC)] */
            if (ww.fmt_size == 0x14 && ww.format == 0x0069) {
                if (ww.channels > 2) goto fail; /* unlikely but just in case */
                if (ww.big_endian) goto fail; /* unsure */
                vgmstream->coding_type = coding_XBOX_IMA;
                vgmstream->layout_type = layout_none;
                vgmstream->interleave_block_size = 0;
            }

            if (ww.prefetch) {
                ww.data_size = ww.file_size - ww.data_offset;
            }

            vgmstream->num_samples = xbox_ima_bytes_to_samples(ww.data_size, ww.channels);
            break;

#ifdef VGM_USE_VORBIS
        case VORBIS: { /* common */
            /* Wwise uses custom Vorbis, which changed over time (config must be detected to pass to the decoder). */
            off_t data_offsets, block_offsets;
            size_t setup_offset, audio_offset;
            vorbis_custom_config cfg = {0};

            cfg.channels = ww.channels;
            cfg.sample_rate = ww.sample_rate;
            cfg.big_endian = ww.big_endian;
            cfg.stream_end = ww.data_offset + ww.data_size;

            if (ww.block_size != 0 || ww.bits_per_sample != 0) goto fail; /* always 0 for Worbis */

            /* autodetect format (fields are mostly common, see the end of the file) */
            if (ww.vorb_offset) {
                /* older Wwise (~<2012) */

                switch(ww.vorb_size) {
                    case 0x2C: /* earliest (~2009) [The Lord of the Rings: Conquest (PC)] */
                    case 0x28: /* early (~2009) [UFC Undisputed 2009 (PS3), some EVE Online Apocrypha (PC)] */
                        data_offsets = 0x18;
                        block_offsets = 0; /* no need, full headers are present */
                        cfg.header_type = WWV_TYPE_8;
                        cfg.packet_type = WWV_STANDARD;
                        cfg.setup_type = WWV_HEADER_TRIAD;
                        break;

                    case 0x34:  /* common (2010~2011) [The King of Fighters XII (PS3), Assassin's Creed II (X360)] */
                    case 0x32:  /* rare (mid 2011) [Saints Row the 3rd (PC)] */
                        data_offsets = 0x18;
                        block_offsets = 0x30;
                        cfg.header_type = WWV_TYPE_6;
                        cfg.packet_type = WWV_STANDARD;
                        cfg.setup_type = WWV_EXTERNAL_CODEBOOKS; /* setup_type will be corrected later */
                        break;

                    case 0x2a:  /* uncommon (mid 2011) [inFamous 2 (PS3), Captain America: Super Soldier (X360)] */
                        data_offsets = 0x10;
                        block_offsets = 0x28;
                        cfg.header_type = WWV_TYPE_2;
                        cfg.packet_type = WWV_MODIFIED;
                        cfg.setup_type = WWV_EXTERNAL_CODEBOOKS;
                        break;

                    default:
                        VGM_LOG("WWISE: unknown vorb size 0x%x\n", ww.vorb_size);
                        goto fail;
                }

                vgmstream->num_samples = read_s32(ww.vorb_offset + 0x00, sf);
                setup_offset = read_u32(ww.vorb_offset + data_offsets + 0x00, sf); /* within data (0 = no seek table) */
                audio_offset = read_u32(ww.vorb_offset + data_offsets + 0x04, sf); /* within data */
                if (block_offsets) {
                    cfg.blocksize_1_exp = read_u8(ww.vorb_offset + block_offsets + 0x00, sf); /* small */
                    cfg.blocksize_0_exp = read_u8(ww.vorb_offset + block_offsets + 0x01, sf); /* big */
                }
                ww.data_size -= audio_offset;


                /* detect normal packets */
                if (ww.vorb_size == 0x2a) {
                    /* almost all blocksizes are 0x08+0x0B except a few with 0x0a+0x0a [Captain America: Super Soldier (X360) voices/sfx] */
                    if (cfg.blocksize_0_exp == cfg.blocksize_1_exp)
                        cfg.packet_type = WWV_STANDARD;
                }

                /* detect setup type:
                 * - full inline: ~2009, ex. The King of Fighters XII (X360), The Saboteur (PC)
                 * - trimmed inline: ~2010, ex. Army of Two: 40 days (X360) some multiplayer files
                 * - external: ~2010, ex. Assassin's Creed Brotherhood (X360), Dead Nation (X360) */
                if (ww.vorb_size == 0x34) {
                    size_t setup_size = read_u16  (start_offset + setup_offset + 0x00, sf);
                    uint32_t setup_id = read_u32be(start_offset + setup_offset + 0x06, sf);

                    /* if the setup after header starts with "(data)BCV" it's an inline codebook) */
                    if ((setup_id & 0x00FFFFFF) == get_id32be("\0BCV")) {
                        cfg.setup_type = WWV_FULL_SETUP;
                    }
                    /* if the setup is suspiciously big it's probably trimmed inline codebooks */
                    else if (setup_size > 0x200) { /* an external setup it's ~0x100 max + some threshold */
                        cfg.setup_type = WWV_INLINE_CODEBOOKS;
                    }
                }

                vgmstream->codec_data = init_vorbis_custom(sf, start_offset + setup_offset, VORBIS_WWISE, &cfg);
                if (!vgmstream->codec_data) goto fail;
            }
            else {
                /* newer Wwise (>2012) */
                off_t extra_offset = ww.fmt_offset + 0x18; /* after flag + channels */

                switch(ww.extra_size) {
                    case 0x30:
                        data_offsets = 0x10;
                        block_offsets = 0x28;
                        cfg.header_type = WWV_TYPE_2;
                        cfg.packet_type = WWV_MODIFIED;

                        /* setup not detectable by header, so we'll try both; libvorbis should reject wrong codebooks
                         * - standard: early (<2012), ex. The King of Fighters XIII (X360)-2011/11, .ogg (cbs are from aoTuV, too)
                         * - aoTuV603: later (>2012), ex. Sonic & All-Stars Racing Transformed (PC)-2012/11, .wem */
                        cfg.setup_type  = ww.is_wem ? WWV_AOTUV603_CODEBOOKS : WWV_EXTERNAL_CODEBOOKS; /* aoTuV came along .wem */
                        break;

                    default:
                        VGM_LOG("WWISE: unknown extra size 0x%x\n", ww.vorb_size);
                        goto fail;
                }

                vgmstream->num_samples = read_s32(extra_offset + 0x00, sf);
                setup_offset = read_u32(extra_offset + data_offsets + 0x00, sf); /* within data */
                audio_offset = read_u32(extra_offset + data_offsets + 0x04, sf); /* within data */
                cfg.blocksize_1_exp = read_u8(extra_offset + block_offsets + 0x00, sf); /* small */
                cfg.blocksize_0_exp = read_u8(extra_offset + block_offsets + 0x01, sf); /* big */
                ww.data_size -= audio_offset;

                /* mutant .wem with metadata (voice strings/etc) between seek table and vorbis setup [Gears of War 4 (PC)] */
                if (ww.meta_offset) {
                    /* 0x00: original setup_offset */
                    setup_offset += read_u32(ww.meta_offset + 0x04, sf); /* metadata size */
                }

                /* detect normal packets */
                if (ww.extra_size == 0x30) {
                    /* almost all blocksizes are 0x08+0x0B except some with 0x09+0x09 [Oddworld New 'n' Tasty! (PSV)] */
                    if (cfg.blocksize_0_exp == cfg.blocksize_1_exp)
                        cfg.packet_type = WWV_STANDARD;
                }

                /* try with the selected codebooks */
                vgmstream->codec_data = init_vorbis_custom(sf, start_offset + setup_offset, VORBIS_WWISE, &cfg);
                if (!vgmstream->codec_data) {
                    /* codebooks failed: try again with the other type */
                    cfg.setup_type  = ww.is_wem ? WWV_EXTERNAL_CODEBOOKS : WWV_AOTUV603_CODEBOOKS;
                    vgmstream->codec_data = init_vorbis_custom(sf, start_offset + setup_offset, VORBIS_WWISE, &cfg);
                    if (!vgmstream->codec_data) goto fail;
                }
            }
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
            vgmstream->codec_endian = ww.big_endian;

            start_offset += audio_offset;

            /* Vorbis is VBR so this is very approximate percent, meh */
            if (ww.prefetch) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
            }

            break;
        }
#endif

        case DSP: { /* Wii/3DS/WiiU */
            //if (ww.fmt_size != 0x28 && ww.fmt_size != ?) goto fail; /* old, new */
            if (ww.bits_per_sample != 4) goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08; /* ww.block_size = 0x8 in older Wwise, samples per block in newer Wwise */

            /* find coef position */
            if (ww.wiih_offset) { /* older */
                vgmstream->num_samples = dsp_bytes_to_samples(ww.data_size, ww.channels);
                if (ww.wiih_size != 0x2e * ww.channels) goto fail;

                if (is_dsp_full_interleave(sf, &ww, ww.wiih_offset, 1))
                    vgmstream->interleave_block_size = ww.data_size / 2;
            }
            else if (ww.extra_size == 0x0c + ww.channels * 0x2e) { /* newer */
                vgmstream->num_samples = read_s32(ww.fmt_offset + 0x18, sf);
                ww.wiih_offset = ww.fmt_offset + 0x1c;
                ww.wiih_size = 0x2e * ww.channels;

                if (is_dsp_full_interleave(sf, &ww, ww.wiih_offset, 0)) /* less common */
                    vgmstream->interleave_block_size = ww.data_size / 2;
            }
            else {
                goto fail;
            }

            if (ww.prefetch) {
                ww.data_size = ww.file_size - ww.data_offset;
                vgmstream->num_samples = dsp_bytes_to_samples(ww.data_size, ww.channels);
            }

            /* for some reason all(?) DSP .wem do full loops (even mono/jingles/etc) but
             * several tracks do loop like this, so disable it for short-ish tracks */
            if (ww.loop_flag && vgmstream->loop_start_sample == 0 &&
                    vgmstream->loop_end_sample < 20*ww.sample_rate) { /* in seconds */
                vgmstream->loop_flag = 0;
            }

            dsp_read_coefs(vgmstream, sf, ww.wiih_offset + 0x00, 0x2e, ww.big_endian);
            dsp_read_hist (vgmstream, sf, ww.wiih_offset + 0x24, 0x2e, ww.big_endian);

            break;
        }

#ifdef VGM_USE_FFMPEG
        case XMA2: { /* X360/XBone */
            uint8_t buf[0x100];
            int bytes;

            /* endian check should be enough */
            //if (ww.fmt_size != ...) goto fail; /* XMA1 0x20, XMA2old: 0x34, XMA2new: 0x40, XMA2 Guitar Hero Live/padded: 0x64, etc */

            /* only Wwise XMA: X360=BE, or XBone=LE+wem (real X360 XMA are LE and parsed elsewhere) */
            if (!(ww.big_endian || (!ww.big_endian && check_extensions(sf,"wem,bnk"))))
                goto fail;

            if (ww.xma2_offset) { /* older */
                bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf, sizeof(buf), ww.xma2_offset, ww.xma2_size, ww.data_size, sf);
            }
            else { /* newer */
                bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, sizeof(buf), ww.fmt_offset, ww.fmt_size, ww.data_size, sf, ww.big_endian);
            }

            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, ww.data_offset,ww.data_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ww.num_samples; /* set while parsing XMAWAVEFORMATs */

            /* Wwise loops are always pre-adjusted (old or new) and only num_samples is off */
            xma_fix_raw_samples(vgmstream, sf, ww.data_offset, ww.data_size, ww.xma2_offset ? ww.xma2_offset : ww.fmt_offset, 1,0);

            /* XMA is VBR so this is very approximate percent, meh */
            if (ww.prefetch) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                //todo data size, call function
            }

            break;
        }

        case XWMA: { /* X360 */
            if (ww.fmt_size != 0x18) goto fail;
            if (!ww.big_endian) goto fail; /* must be from Wwise X360 (PC LE XWMA is parsed elsewhere) */

            vgmstream->codec_data = init_ffmpeg_xwma(sf, ww.data_offset, ww.data_size, ww.format, ww.channels, ww.sample_rate, ww.avg_bitrate, ww.block_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            //if (ww.prefetch) {
            //    ww.data_size = ww.file_size - ww.data_offset;
            //}

            /* seek table seems BE dpds */
            vgmstream->num_samples = xwma_dpds_get_samples(sf, ww.seek_offset, ww.seek_size, ww.channels, ww.big_endian);
            if (!vgmstream->num_samples)
                vgmstream->num_samples = xwma_get_samples(sf, ww.data_offset, ww.data_size, ww.format, ww.channels, ww.sample_rate, ww.block_size);

            /* XWMA is VBR so this is very approximate percent, meh */
            if (ww.prefetch) { /* Guardians of Middle Earth (X360) */
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
            }

            break;
        }

        case AAC: {     /* iOS/Mac */
            ffmpeg_codec_data * ffmpeg_data = NULL;

            if (ww.fmt_size != 0x24) goto fail;
            if (ww.block_size != 0 || ww.bits_per_sample != 0) goto fail;

            /* extra: size 0x12, unknown values */

            ffmpeg_data = init_ffmpeg_offset(sf, ww.data_offset,ww.data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ffmpeg_get_samples(ffmpeg_data); //todo correct?
            break;
        }

        case OPUSNX: {  /* Switch */
            size_t skip;
            size_t seek_size;

            if (ww.fmt_size != 0x28) goto fail;
            /* values up to 0x14 seem fixed and similar to HEVAG's (block_size 0x02/04, bits_per_sample 0x10) */

            vgmstream->num_samples = read_s32(ww.fmt_offset + 0x18, sf);
            /* 0x1c: null?
             * 0x20: data_size without seek_size */
            seek_size = read_u32(ww.fmt_offset + 0x24, sf);

            start_offset += seek_size;
            ww.data_size -= seek_size;

            skip = switch_opus_get_encoder_delay(start_offset, sf); /* should be 120 */

            /* some voices have original sample rate but OPUS can only do 48000 (ex. Mario Kart Home Circuit 24khz) */
            if (vgmstream->sample_rate != 48000) {
                vgmstream->sample_rate = 48000;
                vgmstream->num_samples = switch_opus_get_samples(start_offset,ww.data_size, sf); /* also original's */
                vgmstream->num_samples -= skip;
            }

            /* OPUS is VBR so this is very approximate percent, meh */
            if (ww.prefetch) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                ww.data_size = ww.file_size - start_offset;
            }

            vgmstream->codec_data = init_ffmpeg_switch_opus(sf, start_offset,ww.data_size, ww.channels, skip, vgmstream->sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case OPUS: { /* fully standard Ogg Opus [Girl Cafe Gun (Mobile), Gears 5 (PC)] */
            if (ww.block_size != 0 || ww.bits_per_sample != 0) goto fail;

            /* extra: size 0x12 */
            vgmstream->num_samples = read_s32(ww.fmt_offset + 0x18, sf);
            /* 0x1c: stream size without OggS? */
            /* 0x20: full samples (without encoder delay) */

            /* OPUS is VBR so this is very approximate percent, meh */
            if (ww.prefetch) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                ww.data_size = ww.file_size - start_offset;
            }

            /* mutant .wem with metadata (voice strings/etc) at data start [Gears 5 (PC)] */
            if (ww.meta_offset) {
                /* 0x00: original setup_offset? (0x00 for Opus) */
                uint32_t meta_skip = read_u32(ww.meta_offset + 0x04, sf);

                ww.data_offset += meta_skip;
                ww.data_size -= meta_skip;
            }

            vgmstream->codec_data = init_ffmpeg_offset(sf, ww.data_offset, ww.data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case OPUSCPR: { /* CD Project RED's Ogg Opus masquerading as PCMEX [Cyberpunk 2077 (PC)] */
            if (ww.bits_per_sample != 16) goto fail;

            /* original data_size doesn't look related to samples or anything */
            ww.data_size = ww.file_size - ww.data_offset;

            vgmstream->codec_data = init_ffmpeg_offset(sf, ww.data_offset, ww.data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* FFmpeg's samples seem correct, otherwise see ogg_opus.c for getting samples. */
            vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data);
            break;
        }

        case OPUSWW: { /* updated Opus [Assassin's Creed Valhalla (PC)] */
            int mapping;
            opus_config cfg = {0};

            if (ww.block_size != 0 || ww.bits_per_sample != 0) goto fail;
            if (!ww.seek_offset) goto fail;
            if (ww.channels > 8) goto fail; /* mapping not defined */

            cfg.channels = ww.channels;
            cfg.table_offset = ww.seek_offset;

            /* extra: size 0x10 (though last 2 fields are beyond, AK plz) */
            /* 0x12: samples per frame */
            vgmstream->num_samples = read_s32(ww.fmt_offset + 0x18, sf);
            cfg.table_count = read_u32(ww.fmt_offset + 0x1c, sf); /* same as seek size / 2 */
            cfg.skip = read_u16(ww.fmt_offset + 0x20, sf);
            /* 0x22: codec version */
            mapping = read_u8(ww.fmt_offset + 0x23, sf);

            if (read_u8(ww.fmt_offset + 0x22, sf) != 1)
                goto fail;

            /* OPUS is VBR so this is very approximate percent, meh */
            if (ww.prefetch) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                ww.data_size = ww.file_size - start_offset;
            }

            /* AK does some wonky implicit config for multichannel */
            if (mapping == 1 && ww.channel_type == 1) { /* only allowed values ATM, set when >2ch */
                static const int8_t mapping_matrix[8][8] = {
                    { 0, 0, 0, 0, 0, 0, 0, 0, },
                    { 0, 1, 0, 0, 0, 0, 0, 0, },
                    { 0, 2, 1, 0, 0, 0, 0, 0, },
                    { 0, 1, 2, 3, 0, 0, 0, 0, },
                    { 0, 4, 1, 2, 3, 0, 0, 0, },
                    { 0, 4, 1, 2, 3, 5, 0, 0, },
                    { 0, 6, 1, 2, 3, 4, 5, 0, },
                    { 0, 6, 1, 2, 3, 4, 5, 7, },
                };
                int i;

                /* find coupled OPUS streams (internal streams using 2ch) */
                switch(ww.channel_layout) {
                    case mapping_7POINT1_surround:  cfg.coupled_count = 3; break;   /* 2ch+2ch+2ch+1ch+1ch, 5 streams */
                    case mapping_5POINT1_surround:                                  /* 2ch+2ch+1ch+1ch, 4 streams */
                    case mapping_QUAD_side:         cfg.coupled_count = 2; break;   /* 2ch+2ch, 2 streams */
                    case mapping_2POINT1_xiph:                                      /* 2ch+1ch, 2 streams */
                    case mapping_STEREO:            cfg.coupled_count = 1; break;   /* 2ch, 1 stream */
                    default:                        cfg.coupled_count = 0; break;   /* 1ch, 1 stream */
                    //TODO: AK OPUS doesn't seem to handle others mappings, though AK's .h imply they exist (uses 0 coupleds?)
                }

                /* total number internal OPUS streams (should be >0) */
                cfg.stream_count = ww.channels - cfg.coupled_count;

                /* channel assignments */
                for (i = 0; i < ww.channels; i++) {
                    cfg.channel_mapping[i] = mapping_matrix[ww.channels - 1][i];
                }
            }

            /* Wwise Opus saves all frame sizes in the seek table */
            vgmstream->codec_data = init_ffmpeg_wwise_opus(sf, ww.data_offset, ww.data_size, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case HEVAG: /* PSV */
            /* changed values, another bizarre Wwise quirk */
            //ww.block_size /* unknown (1ch=2, 2ch=4) */
            //ww.bits_per_sample; /* unknown (0x10) */
            //if (ww.bits_per_sample != 4) goto fail;

            if (ww.fmt_size != 0x18) goto fail;
            if (ww.big_endian) goto fail;

            /* extra data (size 0x06)
             * 0x00: samples per block (0x1c)
             * 0x02: channel config (again?) */

            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(ww.data_size, ww.channels);
            break;

#ifdef VGM_USE_ATRAC9
        case ATRAC9: {  /* PSV/PS4 */
            atrac9_config cfg = {0};

            if (ww.fmt_size != 0x24) goto fail;
            if (ww.extra_size != 0x12) goto fail;

            /* extra data
             * 0x00: samples per subframe?
             * 0x02: channel config (again?)
             * 0x06: config
             * 0x0a: samples
             * 0x0e: encoder delay? (same as samples per subframe?)
             * 0x10: decoder delay? (PS4 only, 0 on Vita?) */

            cfg.channels = ww.channels;
            cfg.config_data = read_u32be(ww.fmt_offset + 0x18,sf);
            cfg.encoder_delay = read_u16(ww.fmt_offset + 0x20,sf);
            /* PS4 value at 0x22 looks like encoder delay, but using it removes too many
             * samples [DmC: Definitive Edition (PS4)] */

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = read_s32(ww.fmt_offset + 0x1c, sf);

            if (ww.prefetch) {
                vgmstream->num_samples = atrac9_bytes_to_samples_cfg(ww.file_size - start_offset, cfg.config_data);
            }

            break;
        }
#endif

        case PTADPCM: /* newer ADPCM [Bayonetta 2 (Switch), Genshin Impact (PC)]  */
            if (ww.bits_per_sample != 4) goto fail;
            if (ww.block_size != 0x24 * ww.channels && ww.block_size != 0x104 * ww.channels) goto fail;

            vgmstream->coding_type = coding_PTADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = ww.block_size / ww.channels;
          //vgmstream->codec_endian = ww.big_endian; //?

            if (ww.prefetch) {
                ww.data_size = ww.file_size - ww.data_offset;
            }

            vgmstream->num_samples = ptadpcm_bytes_to_samples(ww.data_size, ww.channels, vgmstream->interleave_block_size);
            break;

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static int is_dsp_full_interleave(STREAMFILE* sf, wwise_header* ww, off_t coef_offset, int full_detection) {
    /* older (bank ~v48) Wwise use full interleave for memory (in .bnk) files, but
     * detection from the .wem side is problematic [Punch Out!! (Wii)-old, Luigi's Mansion 2 (3DS)-new]
     * - prefetch point to streams = normal
     * - .bnk would be memory banks = full
     * - otherwise small-ish sizes, stereo, with initial predictors for the
     *   second channel matching half size = full
     * some files aren't detectable like this though, when predictors are 0
     * (but since memory wem aren't that used this shouldn't be too common) */

    if (ww->prefetch)
        return 0;

    if (ww->channels == 1)
        return 0;

    if (ww->is_bnk)
        return 1;

    if (ww->data_size > 0x30000)
        return 0;

    /* skip reading data if possible */
    if (full_detection) {
        uint16_t head_ps2 = read_u16be(coef_offset + 1 * 0x2e + 0x22, sf); /* ch2's initial predictor */
        uint16_t init_ps2 = read_u8(ww->data_offset + 0x08, sf); /* at normal interleave */
        uint16_t half_ps2 = read_u8(ww->data_offset + ww->data_size / 2, sf); /* at full interleave */
        //;VGM_LOG("WWISE: DSP head2=%x, init2=%x, half2=%x\n", head_ps2, init_ps2, half_ps2);
        if (head_ps2 != init_ps2 && head_ps2 == half_ps2) {
            return 1;
        }
    }

    return 0;
}


static int parse_wwise(STREAMFILE* sf, wwise_header* ww) {
    read_u32_t read_u32;
    read_u16_t read_u16;

    /* Wwise honors machine's endianness (PC=RIFF, X360=RIFX --unlike XMA) */
    ww->big_endian = is_id32be(0x00,sf, "RIFX"); /* RIFF size not useful to detect, see below */
    if (ww->big_endian) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }

    ww->file_size = get_streamfile_size(sf);

#if 0
    /* Wwise's RIFF size is often wonky, seemingly depending on codec:
     * - PCM, IMA/PTADPCM, VORBIS, AAC, OPUSNX/OPUS: correct
     * - DSP, XWMA, ATRAC9: almost always slightly smaller (around 0x50)
     * - HEVAG: very off
     * - XMA2: exact file size
     * - some RIFX have LE size
     * Value is ignored by AK's parser (set to -1).
     * (later we'll validate "data" which fortunately is correct)
     */
    if (read_u32(0x04,sf) + 0x04 + 0x04 != ww->file_size) {
        VGM_LOG("WWISE: bad riff size\n");
        goto fail;
    }
#endif

    if (!is_id32be(0x08,sf, "WAVE") &&
        !is_id32be(0x08,sf, "XWMA"))
        goto fail;


    /* parse chunks (reads once linearly) */
    {
        chunk_t rc = {0};
        uint32_t file_size = get_streamfile_size(sf);;

        /* chunks are even-aligned and don't need to add padding byte, unlike real RIFFs */
        rc.be_size = ww->big_endian;
        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {

            switch(rc.type) {
                case 0x666d7420: /* "fmt " */
                    ww->fmt_offset = rc.offset;
                    ww->fmt_size = rc.size;
                    break;
                case 0x584D4132: /* "XMA2" */
                    ww->xma2_offset = rc.offset;
                    ww->xma2_size = rc.size;
                    break;
                case 0x64617461: /* "data" */
                    ww->data_offset = rc.offset;
                    ww->data_size = rc.size;
                    break;
                case 0x766F7262: /* "vorb" */
                    ww->vorb_offset = rc.offset;
                    ww->vorb_size = rc.size;
                    break;
                case 0x57696948: /* "WiiH" */
                    ww->wiih_offset = rc.offset;
                    ww->wiih_size = rc.size;
                    break;
                case 0x7365656B: /* "seek" */
                    ww->seek_offset = rc.offset;
                    ww->seek_size = rc.size;
                    break;
                case 0x736D706C: /* "smpl" */
                    ww->smpl_offset = rc.offset;
                    ww->smpl_size = rc.size;
                    break;
                case 0x6D657461: /* "meta" */
                    ww->meta_offset = rc.offset;
                    ww->meta_size = rc.size;
                    break;

                case 0x66616374: /* "fact" */
                    /* Wwise never uses fact, but if somehow some file does uncomment the following: */
                    //if (size == 0x10 && read_u32be(offset + 0x04, sf) == 0x4C794E20) /* "LyN " */
                    //    goto fail; /* ignore LyN RIFF */
                    goto fail;

                /* "XMAc": rare XMA2 physical loop regions (loop_start_b, loop_end_b, loop_subframe_data)
                 *         Can appear even in the file doesn't loop, maybe it's meant to be the playable physical region */
                /* "LIST": leftover 'cue' info from OG .wavs (ex. loop starts in Platinum Games) */
                /* "JUNK": optional padding for aligment (0-size JUNK exists too) */
                /* "akd ": extra info for Wwise? (wave peaks/loudness/HDR envelope?) */
                default:
                    /* mainly for incorrectly ripped wems, but should allow truncated wems
                     * (could also check that fourcc is ASCII)  */
                    if (rc.offset + rc.size > file_size) {
                        vgm_logi("WWISE: broken .wem (bad extract?)\n");
                        goto fail;
                    }

                    break;
            }
        }
    }

    /* use extension as a guide for certain cases */
    ww->is_wem = check_extensions(sf,"wem,bnk");

    /* parse format (roughly spec-compliant but some massaging is needed) */
    if (ww->xma2_offset) {
        /* pseudo-XMA2WAVEFORMAT, "fmt"+"XMA2" (common) or only "XMA2" [Too Human (X360)] */
        ww->format = 0x0165; /* signal for below */
        xma2_parse_xma2_chunk(sf,
                ww->xma2_offset, &ww->channels, &ww->sample_rate, &ww->loop_flag,
                &ww->num_samples, &ww->loop_start_sample, &ww->loop_end_sample);
    }
    else {
        /* pseudo-WAVEFORMATEX */
        if (ww->fmt_size < 0x10)
            goto fail;
        ww->format           = read_u16(ww->fmt_offset + 0x00,sf);
        ww->channels         = read_u16(ww->fmt_offset + 0x02,sf);
        ww->sample_rate      = read_u32(ww->fmt_offset + 0x04,sf);
        ww->avg_bitrate      = read_u32(ww->fmt_offset + 0x08,sf);
        ww->block_size       = read_u16(ww->fmt_offset + 0x0c,sf);
        ww->bits_per_sample  = read_u16(ww->fmt_offset + 0x0e,sf);
        if (ww->fmt_size > 0x10 && ww->format != 0x0165 && ww->format != 0x0166) /* ignore XMAWAVEFORMAT */
            ww->extra_size   = read_u16(ww->fmt_offset + 0x10,sf);
        if (ww->extra_size >= 0x06) { /* always present (actual RIFFs only have it in WAVEFORMATEXTENSIBLE) */
            /* mostly WAVEFORMATEXTENSIBLE's bitmask (see AkSpeakerConfig.h) */
            ww->channel_layout = read_u32(ww->fmt_offset + 0x14,sf);
            /* later games (+2018?) have a pseudo-format instead to handle more cases:
             * - 8b: uNumChannels
             * - 4b: eConfigType  (0=none, 1=standard, 2=ambisonic)
             * - 19b: uChannelMask */
            if ((ww->channel_layout & 0xFF) == ww->channels) {
                ww->channel_type = (ww->channel_layout >> 8) & 0x0F;
                ww->channel_layout = (ww->channel_layout >> 12);
            }
        }

        if (ww->format == 0x0166) { /* XMA2WAVEFORMATEX in fmt */
            xma2_parse_fmt_chunk_extra(sf, ww->fmt_offset, &ww->loop_flag,
                    &ww->num_samples, &ww->loop_start_sample, &ww->loop_end_sample, ww->big_endian);
        }
    }

    /* common loops ("XMA2" chunks already read them) */
    if (ww->smpl_offset) {
        if (ww->smpl_size >= 0x34
                && read_u32(ww->smpl_offset + 0x1c, sf) == 1           /* loop count */
                && read_u32(ww->smpl_offset + 0x24 + 0x04, sf) == 0) { /* loop type */
            ww->loop_flag = 1;
            ww->loop_start_sample = read_u32(ww->smpl_offset + 0x24 + 0x8, sf);
            ww->loop_end_sample   = read_u32(ww->smpl_offset + 0x24 + 0xc, sf) + 1; /* +1 like standard RIFF */
        }
    }

    if (!ww->data_offset)
        goto fail;


    /* format to codec */
    switch(ww->format) {
        case 0x0001: ww->codec = PCM; break; /* older Wwise */
        case 0x0002: ww->codec = IMA; break; /* newer Wwise (variable, probably means "platform's ADPCM") */
        case 0x0069: ww->codec = IMA; break; /* older Wwise [Spiderman Web of Shadows (X360), LotR Conquest (PC)] */
        case 0x0161: ww->codec = XWMA; break; /* WMAv2 */
        case 0x0162: ww->codec = XWMA; break; /* WMAPro */
        case 0x0165: ww->codec = XMA2; break; /* XMA2-chunk XMA (Wwise doesn't use XMA1) */
        case 0x0166: ww->codec = XMA2; break; /* fmt-chunk XMA */
        case 0xAAC0: ww->codec = AAC; break;
        case 0xFFF0: ww->codec = DSP; break;
        case 0xFFFB: ww->codec = HEVAG; break; /* "VAG" */
        case 0xFFFC: ww->codec = ATRAC9; break;
        case 0xFFFE: ww->codec = PCM; break; /* "PCM for Wwise Authoring" */
        case 0xFFFF: ww->codec = VORBIS; break;
        case 0x3039: ww->codec = OPUSNX; break; /* renamed from "OPUS" on Wwise 2018.1 */
        case 0x3040: ww->codec = OPUS; break;
        case 0x3041: ww->codec = OPUSWW; break; /* "OPUS_WEM", added on Wwise 2019.2.3, replaces OPUS */
        case 0x8311: ww->codec = PTADPCM; break; /* added on Wwise 2019.1, replaces IMA */
        default:
            /* some .wav may end up here, only report in .wem cases (newer codecs) */
            if (ww->is_wem)
                vgm_logi("WWISE: unknown codec 0x%04x (report)\n", ww->format);
            goto fail;
    }

    /* identify system's ADPCM */
    if (ww->format == 0x0002) {
        if (ww->extra_size == 0x0c + ww->channels * 0x2e) {
            /* newer Wwise DSP with coefs [Epic Mickey 2 (Wii), Batman Arkham Origins Blackgate (3DS)] */
            ww->codec = DSP;
        }
        else if (ww->extra_size == 0x0a && ww->wiih_offset) { /* WiiH */
            /* few older Wwise DSP with num_samples in extra_size [Tony Hawk: Shred (Wii)] */
            ww->codec = DSP;
        }
        else if (ww->block_size == 0x104 * ww->channels) {
            /* Bayonetta 2 (Switch) */
            ww->codec = PTADPCM;
        }
    }


    /* Some Wwise .bnk (RAM) files have truncated, prefetch mirrors of another file, that
     * play while the rest of the real stream loads. We'll add basic support to avoid
     * complaints of this or that .wem not playing */
    if (ww->data_offset + ww->data_size > ww->file_size) {
        //;VGM_LOG("WWISE: truncated data size (prefetch): (real=0x%x > riff=0x%x)\n", ww->data_size, ww->file_size);

        /* catch wrong rips as truncated tracks' file_size should be much smaller than data_size,
         * but it's possible to pre-fetch small files too [Punch Out!! (Wii)] */
        if (ww->data_offset + ww->data_size - ww->file_size < 0x5000 && ww->file_size > 0x10000) {
            vgm_logi("WWISE: wrong expected size (re-rip?)\n");
            goto fail;
        }

        if (ww->codec == PCM || ww->codec == IMA || ww->codec == VORBIS || ww->codec == DSP || ww->codec == XMA2 ||
            ww->codec == OPUSNX || ww->codec == OPUS || ww->codec == OPUSWW || ww->codec == PTADPCM || ww->codec == XWMA || ww->codec == ATRAC9) {
            ww->prefetch = 1; /* only seen those, probably all exist (missing XWMA, AAC, HEVAG) */
        } else {
            vgm_logi("WWISE: wrong expected size, maybe prefetch (report)\n");
            goto fail;
        }
    }


    /* Cyberpunk 2077 has some mutant .wem, with proper Wwise header and PCMEX but data is standard OPUS.
     * From init bank and CAkSound's sources, those may be piped through their plugins. They come in
     * .opuspak (no names), have wrong riff/data sizes and only seem used for sfx (other audio is Vorbis). */
    if (ww->format == 0xFFFE && ww->prefetch) {
        if (is_id32be(ww->data_offset + 0x00, sf, "OggS")) {
            ww->codec = OPUSCPR;
        }
    }

    return 1;
fail:
    return 0;
}


/* VORBIS FORMAT RESEARCH */
/*
- old format
"fmt" size 0x28, extra size 0x16 / size 0x18, extra size 0x06
0x18-24 (16): ? (fixed: 0x01000000 00001000 800000AA 00389B71)  [removed when extra size is 0x06]

"vorb" size 0x34
0x00 (4): dwTotalPCMFrames
0x04 (4): skip samples?
0x08 (4): LoopInfo.uLoopBeginExtra? (present if loop)
0x0c (4): LoopInfo.dwLoopStartPacketOffset (data start, or loop start when "smpl" is present)
0x10 (4): LoopInfo.uLoopEndExtra? (0..~0x400)
0x14 (4): LoopInfo.dwLoopEndPacketOffset?
0x18 (4): dwSeekTableSize (0 = no seek table)
0x1c (4): dwVorbisDataOffset (offset within data)
0x20 (2): uMaxPacketSize (not including header)
0x22 (2): uLastGranuleExtra (0..~0x100)
0x24 (4): dwDecodeAllocSize (0~0x5000)
0x28 (4): dwDecodeX64AllocSize (mid, 0~0x5000)
0x2c (4): uHashCodebook? (shared by several .wem a game, but not all need to share it)
0x30 (1): uBlockSizes[0] (blocksize_1_exp, small)
0x31 (1): uBlockSizes[1] (blocksize_0_exp, large)
0x32 (2): empty

"vorb" size 0x28 / 0x2c / 0x2a
0x00 (4): dwTotalPCMFrames
0x04 (4): LoopInfo.dwLoopStartPacketOffset (data start, or loop start when "smpl" is present)
0x08 (4): LoopInfo.dwLoopEndPacketOffset (data end, or loop end when "smpl" is present)
0x0c (2): ? (small, 0..~0x400) [(4) when size is 0x2C]
0x10 (4): dwSeekTableSize (0 = no seek table)
0x14 (4): dwVorbisDataOffset (offset within data)
0x18 (2): uMaxPacketSize (not including header)
0x1a (2): uLastGranuleExtra (0..~0x100) [(4) when size is 0x2C]
0x1c (4): dwDecodeAllocSize (0~0x5000)
0x20 (4): dwDecodeX64AllocSize (0~0x5000)
0x24 (4): uHashCodebook? (shared by several .wem a game, but not all need to share it)
0x28 (1): uBlockSizes[0] (blocksize_1_exp, small) [removed when size is 0x28]
0x29 (1): uBlockSizes[1] (blocksize_0_exp, large) [removed when size is 0x28]

- new format:
"fmt" size 0x42, extra size 0x30
0x18 (4): dwTotalPCMFrames
0x1c (4): LoopInfo.dwLoopStartPacketOffset (data start, or loop start when "smpl" is present)
0x20 (4): LoopInfo.dwLoopEndPacketOffset (data end, or loop end when "smpl" is present)
0x24 (2): LoopInfo.uLoopBeginExtra (small, 0..~0x400)
0x26 (2): LoopInfo.uLoopEndExtra (extra samples after seek?)
0x28 (4): dwSeekTableSize (0 = no seek table)
0x2c (4): dwVorbisDataOffset (offset within data)
0x30 (2): uMaxPacketSize (not including header)
0x32 (2): uLastGranuleExtra (small, 0..~0x100)
0x34 (4): dwDecodeAllocSize (mid, 0~0x5000)
0x38 (4): dwDecodeX64AllocSize (mid, 0~0x5000)
0x40 (1): uBlockSizes[0] (blocksize_1_exp, small)
0x41 (1): uBlockSizes[1] (blocksize_0_exp, large)
*/
