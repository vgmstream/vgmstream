#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


/* Wwise uses a custom RIFF/RIFX header, non-standard enough that it's parsed it here.
 * There is some repetition from other metas, but not enough to bother me.
 *
 * Some info: https://www.audiokinetic.com/en/library/edge/
 */
typedef enum { PCM, IMA, VORBIS, DSP, XMA2, XWMA, AAC, HEVAG, ATRAC9, OPUSNX, OPUS, PTADPCM } wwise_codec;
typedef struct {
    int big_endian;
    size_t file_size;
    int truncated;

    /* chunks references */
    off_t fmt_offset;
    size_t fmt_size;
    off_t data_offset;
    size_t data_size;
    off_t chunk_offset;

    /* standard fmt stuff */
    wwise_codec codec;
    int format;
    int channels;
    int sample_rate;
    int block_align;
    int average_bps;
    int bits_per_sample;
    uint32_t channel_layout;
    size_t extra_size;

    int32_t num_samples;
    int loop_flag;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
} wwise_header;

static int is_dsp_full_interleave(STREAMFILE* sf, wwise_header* ww, off_t coef_offset);


/* Wwise - Audiokinetic Wwise (Wave Works Interactive Sound Engine) middleware */
VGMSTREAM * init_vgmstream_wwise(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    wwise_header ww = {0};
    off_t start_offset, first_offset = 0xc;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* checks */
    /* .wem: newer "Wwise Encoded Media" used after the 2011.2 SDK (~july 2011)
     * .wav: older ADPCM files [Punch Out!! (Wii)]
     * .xma: older XMA files [Too Human (X360), Tron Evolution (X360)]
     * .ogg: older Vorbis files [The King of Fighters XII (X360)]
     * .bnk: Wwise banks for memory .wem detection */
    if (!check_extensions(sf,"wem,wav,lwav,ogg,logg,xma,bnk"))
        goto fail;

    if (read_32bitBE(0x00,sf) != 0x52494646 &&  /* "RIFF" (LE) */
        read_32bitBE(0x00,sf) != 0x52494658)    /* "RIFX" (BE) */
        goto fail;
    if (read_32bitBE(0x08,sf) != 0x57415645 &&  /* "WAVE" */
        read_32bitBE(0x08,sf) != 0x58574D41)    /* "XWMA" */
        goto fail;

    ww.big_endian = read_32bitBE(0x00,sf) == 0x52494658; /* RIFX */
    if (ww.big_endian) { /* Wwise honors machine's endianness (PC=RIFF, X360=RIFX --unlike XMA) */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    ww.file_size = sf->get_size(sf);

#if 0
    /* Wwise's RIFF size is often wonky, seemingly depending on codec:
     * - PCM, IMA/PTADPCM, VORBIS, AAC, OPUSNX/OPUS: correct
     * - DSP, XWMA, ATRAC9: almost always slightly smaller (around 0x50)
     * - HEVAG: very off
     * - XMA2: exact file size
     * - some RIFX have LE size
     * (later we'll validate "data" which fortunately is correct)
     */
    if (read_32bit(0x04,sf)+0x04+0x04 != ww.file_size) {
        VGM_LOG("WWISE: bad riff size (real=0x%x vs riff=0x%x)\n", read_32bit(0x04,sf)+0x04+0x04, ww.file_size);
        goto fail;
    }
#endif

    /* ignore LyN RIFF */
    {
        off_t fact_offset;
        size_t fact_size;

        if (find_chunk(sf, 0x66616374,first_offset,0, &fact_offset,&fact_size, 0, 0)) { /* "fact" */
            if (fact_size == 0x10 && read_32bitBE(fact_offset+0x04, sf) == 0x4C794E20) /* "LyN " */
                goto fail; /* parsed elsewhere */
        }
        /* Wwise doesn't use "fact", though */
    }

    /* parse format (roughly spec-compliant but some massaging is needed) */
    {
        off_t loop_offset;
        size_t loop_size;

        /* find basic chunks */
        if (read_32bitBE(0x0c, sf) == 0x584D4132) { /* "XMA2" with no "fmt" [Too Human (X360)] */
            ww.format = 0x0165; /* signal for below */
        }
        else {
            if (!find_chunk(sf, 0x666d7420,first_offset,0, &ww.fmt_offset,&ww.fmt_size, ww.big_endian, 0)) /* "fmt " */
                goto fail;
            if (ww.fmt_size < 0x12)
                goto fail;
            ww.format = (uint16_t)read_16bit(ww.fmt_offset+0x00,sf);
        }


        if (ww.format == 0x0165) {
            /* pseudo-XMA2WAVEFORMAT ("fmt"+"XMA2" or just "XMA2) */
            if (!find_chunk(sf, 0x584D4132,first_offset,0, &ww.chunk_offset,NULL, ww.big_endian, 0)) /* "XMA2" */
                goto fail;
            xma2_parse_xma2_chunk(sf, ww.chunk_offset,&ww.channels,&ww.sample_rate, &ww.loop_flag, &ww.num_samples, &ww.loop_start_sample, &ww.loop_end_sample);
        }
        else {
            /* pseudo-WAVEFORMATEX */
            ww.channels         = read_16bit(ww.fmt_offset+0x02,sf);
            ww.sample_rate      = read_32bit(ww.fmt_offset+0x04,sf);
            ww.average_bps      = read_32bit(ww.fmt_offset+0x08,sf);/* bytes per sec */
            ww.block_align      = (uint16_t)read_16bit(ww.fmt_offset+0x0c,sf);
            ww.bits_per_sample   = (uint16_t)read_16bit(ww.fmt_offset+0x0e,sf);
            if (ww.fmt_size > 0x10 && ww.format != 0x0165 && ww.format != 0x0166) /* ignore XMAWAVEFORMAT */
                ww.extra_size   = (uint16_t)read_16bit(ww.fmt_offset+0x10,sf);
            if (ww.extra_size >= 0x06) { /* always present (actual RIFFs only have it in WAVEFORMATEXTENSIBLE) */
                /* mostly WAVEFORMATEXTENSIBLE's bitmask (see AkSpeakerConfig.h) */
                ww.channel_layout = read_32bit(ww.fmt_offset+0x14,sf);
                /* later games (+2018?) have a pseudo-format instead to handle more cases:
                 * - 8b: uNumChannels
                 * - 4b: eConfigType  (0=none, 1=standard, 2=ambisonic)
                 * - 19b: uChannelMask */
                if ((ww.channel_layout & 0xFF) == ww.channels) {
                    ww.channel_layout = (ww.channel_layout >> 12);
                }
            }
        }

        /* find loop info ("XMA2" chunks already read them) */
        if (ww.format == 0x0166) { /* XMA2WAVEFORMATEX in fmt */
            ww.chunk_offset = ww.fmt_offset;
            xma2_parse_fmt_chunk_extra(sf, ww.chunk_offset, &ww.loop_flag, &ww.num_samples, &ww.loop_start_sample, &ww.loop_end_sample, ww.big_endian);
        }
        else if (find_chunk(sf, 0x736D706C,first_offset,0, &loop_offset,&loop_size, ww.big_endian, 0)) { /* "smpl", common */
            if (loop_size >= 0x34
                    && read_32bit(loop_offset+0x1c, sf)==1        /* loop count */
                    && read_32bit(loop_offset+0x24+4, sf)==0) {
                ww.loop_flag = 1;
                ww.loop_start_sample = read_32bit(loop_offset+0x24+0x8, sf);
                ww.loop_end_sample   = read_32bit(loop_offset+0x24+0xc, sf) + 1; /* like standard RIFF */
            }
        }
        //else if (find_chunk(sf, 0x4C495354,first_offset,0, &loop_offset,&loop_size, ww.big_endian, 0)) { /*"LIST", common */
        //    /* usually contains "cue"s with sample positions for events (ex. Platinum Games) but no real looping info */
        //}

        /* other Wwise specific chunks:
         * "JUNK": optional padding for aligment (0-size JUNK exists too)
         * "akd ": seem to store extra info for Wwise editor (wave peaks/loudness/HDR envelope?)
         */

        if (!find_chunk(sf, 0x64617461,first_offset,0, &ww.data_offset,&ww.data_size, ww.big_endian, 0)) /* "data" */
            goto fail;
    }

    /* format to codec */
    switch(ww.format) {
        case 0x0001: ww.codec = PCM; break; /* older Wwise */
        case 0x0002: ww.codec = IMA; break; /* newer Wwise (conflicts with MSADPCM, probably means "platform's ADPCM") */
        case 0x0069: ww.codec = IMA; break; /* older Wwise [Spiderman Web of Shadows (X360), LotR Conquest (PC)] */
        case 0x0161: ww.codec = XWMA; break; /* WMAv2 */
        case 0x0162: ww.codec = XWMA; break; /* WMAPro */
        case 0x0165: ww.codec = XMA2; break; /* XMA2-chunk XMA (Wwise doesn't use XMA1) */
        case 0x0166: ww.codec = XMA2; break; /* fmt-chunk XMA */
        case 0xAAC0: ww.codec = AAC; break;
        case 0xFFF0: ww.codec = DSP; break;
        case 0xFFFB: ww.codec = HEVAG; break;
        case 0xFFFC: ww.codec = ATRAC9; break;
        case 0xFFFE: ww.codec = PCM; break; /* "PCM for Wwise Authoring" */
        case 0xFFFF: ww.codec = VORBIS; break;
        case 0x3039: ww.codec = OPUSNX; break; /* later renamed from "OPUS" */
        case 0x3040: ww.codec = OPUS; break;
        case 0x8311: ww.codec = PTADPCM; break; /* newer, rare [Genshin Impact (PC)] */
        default:
            goto fail;
    }

    /* identify system's ADPCM */
    if (ww.format == 0x0002) {
        if (ww.extra_size == 0x0c + ww.channels * 0x2e) {
            /* newer Wwise DSP with coefs [Epic Mickey 2 (Wii), Batman Arkham Origins Blackgate (3DS)] */
            ww.codec = DSP;
        } else if (ww.extra_size == 0x0a && find_chunk(sf, 0x57696948, first_offset,0, NULL,NULL, ww.big_endian, 0)) { /* WiiH */
            /* few older Wwise DSP with num_samples in extra_size [Tony Hawk: Shred (Wii)] */
            ww.codec = DSP;
        } else if (ww.block_align == 0x104 * ww.channels) {
            ww.codec = PTADPCM; /* Bayonetta 2 (Switch) */
        }
    }


    /* Some Wwise .bnk (RAM) files have truncated, prefetch mirrors of another file, that
     * play while the rest of the real stream loads. We'll add basic support to avoid
     * complaints of this or that .wem not playing */
    if (ww.data_offset + ww.data_size > ww.file_size) {
        //VGM_LOG("WWISE: truncated data size (prefetch): (real=0x%x > riff=0x%x)\n", ww.data_size, ww.file_size);

        /* catch wrong rips as truncated tracks' file_size should be much smaller than data_size,
         * but it's possible to pre-fetch small files too [Punch Out!! (Wii)] */
        if (ww.data_offset + ww.data_size - ww.file_size < 0x5000 && ww.file_size > 0x10000) {
            VGM_LOG("WWISE: wrong expected data_size\n");
            goto fail;
        }

        if (ww.codec == PCM || ww.codec == IMA || ww.codec == DSP || ww.codec == VORBIS || ww.codec == XMA2 || ww.codec == OPUSNX || ww.codec == OPUS) {
            ww.truncated = 1; /* only seen those, probably all exist */
        } else {
            VGM_LOG("WWISE: wrong size, maybe truncated\n");
            goto fail;
        }
    }


    start_offset = ww.data_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ww.channels,ww.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ww.sample_rate;
    vgmstream->loop_start_sample = ww.loop_start_sample;
    vgmstream->loop_end_sample = ww.loop_end_sample;
    vgmstream->channel_layout = ww.channel_layout;
    vgmstream->meta_type = meta_WWISE_RIFF;

    switch(ww.codec) {
        case PCM: /* common */
            /* normally riff.c has priority but it's needed when .wem is used */
            if (ww.fmt_size != 0x10 && ww.fmt_size != 0x18 && ww.fmt_size != 0x28) goto fail; /* old, new/Limbo (PC) */
            if (ww.bits_per_sample != 16) goto fail;

            vgmstream->coding_type = (ww.big_endian ? coding_PCM16BE : coding_PCM16LE);
            vgmstream->layout_type = ww.channels > 1 ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = 0x02;

            if (ww.truncated) {
                ww.data_size = ww.file_size - ww.data_offset;
            }

            vgmstream->num_samples = pcm_bytes_to_samples(ww.data_size, ww.channels, ww.bits_per_sample);
            break;

        case IMA: /* common */
            /* slightly modified XBOX-IMA */
            /* Wwise reuses common codec ids (ex. 0x0002 MSADPCM) for IMA so this parser should go AFTER riff.c avoid misdetection */

            if (ww.fmt_size != 0x28 && ww.fmt_size != 0x18) goto fail; /* old, new */
            if (ww.bits_per_sample != 4) goto fail;
            if (ww.block_align != 0x24 * ww.channels) goto fail;

            vgmstream->coding_type = coding_WWISE_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = ww.block_align / ww.channels;
            vgmstream->codec_endian = ww.big_endian;

            if (ww.truncated) {
                ww.data_size = ww.file_size - ww.data_offset;
            }

            vgmstream->num_samples = xbox_ima_bytes_to_samples(ww.data_size, ww.channels);
            break;

#ifdef VGM_USE_VORBIS
        case VORBIS: {  /* common */
            /* Wwise uses custom Vorbis, which changed over time (config must be detected to pass to the decoder). */
            off_t vorb_offset, data_offsets, block_offsets;
            size_t vorb_size, setup_offset, audio_offset;
            vorbis_custom_config cfg = {0};

            cfg.channels = ww.channels;
            cfg.sample_rate = ww.sample_rate;
            cfg.big_endian = ww.big_endian;

            if (ww.block_align != 0 || ww.bits_per_sample != 0) goto fail; /* always 0 for Worbis */

            /* autodetect format (fields are mostly common, see the end of the file) */
            if (find_chunk(sf, 0x766F7262,first_offset,0, &vorb_offset,&vorb_size, ww.big_endian, 0)) { /* "vorb" */
                /* older Wwise (~<2012) */

                switch(vorb_size) {
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
                        VGM_LOG("WWISE: unknown vorb size 0x%x\n", vorb_size);
                        goto fail;
                }

                vgmstream->num_samples = read_32bit(vorb_offset + 0x00, sf);
                setup_offset    = read_32bit(vorb_offset + data_offsets + 0x00, sf); /* within data (0 = no seek table) */
                audio_offset    = read_32bit(vorb_offset + data_offsets + 0x04, sf); /* within data */
                if (block_offsets) {
                    cfg.blocksize_1_exp = read_8bit(vorb_offset + block_offsets + 0x00, sf); /* small */
                    cfg.blocksize_0_exp = read_8bit(vorb_offset + block_offsets + 0x01, sf); /* big */
                }
                ww.data_size -= audio_offset;


                /* detect normal packets */
                if (vorb_size == 0x2a) {
                    /* almost all blocksizes are 0x08+0x0B except a few with 0x0a+0x0a [Captain America: Super Soldier (X360) voices/sfx] */
                    if (cfg.blocksize_0_exp == cfg.blocksize_1_exp)
                        cfg.packet_type = WWV_STANDARD;
                }

                /* detect setup type:
                 * - full inline: ~2009, ex. The King of Fighters XII (X360), The Saboteur (PC)
                 * - trimmed inline: ~2010, ex. Army of Two: 40 days (X360) some multiplayer files
                 * - external: ~2010, ex. Assassin's Creed Brotherhood (X360), Dead Nation (X360) */
                if (vorb_size == 0x34) {
                    size_t setup_size = (uint16_t)read_16bit(start_offset + setup_offset, sf);
                    uint32_t id = (uint32_t)read_32bitBE(start_offset + setup_offset + 0x06, sf);

                    /* if the setup after header starts with "(data)BCV" it's an inline codebook) */
                    if ((id & 0x00FFFFFF) == 0x00424356) { /* 0"BCV" */
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
                int is_wem = check_extensions(sf,"wem");

                switch(ww.extra_size) {
                    case 0x30:
                        data_offsets = 0x10;
                        block_offsets = 0x28;
                        cfg.header_type = WWV_TYPE_2;
                        cfg.packet_type = WWV_MODIFIED;

                        /* setup not detectable by header, so we'll try both; hopefully libvorbis will reject wrong codebooks
                         * - standard: early (<2012), ex. The King of Fighters XIII (X360)-2011/11, .ogg (cbs are from aoTuV, too)
                         * - aoTuV603: later (>2012), ex. Sonic & All-Stars Racing Transformed (PC)-2012/11, .wem */
                        cfg.setup_type  = is_wem ? WWV_AOTUV603_CODEBOOKS : WWV_EXTERNAL_CODEBOOKS; /* aoTuV came along .wem */
                        break;

                    default:
                        VGM_LOG("WWISE: unknown extra size 0x%x\n", vorb_size);
                        goto fail;
                }

                vgmstream->num_samples = read_32bit(extra_offset + 0x00, sf);
                setup_offset = read_32bit(extra_offset + data_offsets + 0x00, sf); /* within data */
                audio_offset = read_32bit(extra_offset + data_offsets + 0x04, sf); /* within data */
                cfg.blocksize_1_exp = read_8bit(extra_offset + block_offsets + 0x00, sf); /* small */
                cfg.blocksize_0_exp = read_8bit(extra_offset + block_offsets + 0x01, sf); /* big */
                ww.data_size -= audio_offset;

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
                    cfg.setup_type  = is_wem ? WWV_EXTERNAL_CODEBOOKS : WWV_AOTUV603_CODEBOOKS;
                    vgmstream->codec_data = init_vorbis_custom(sf, start_offset + setup_offset, VORBIS_WWISE, &cfg);
                    if (!vgmstream->codec_data) goto fail;
                }
            }
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
            vgmstream->codec_endian = ww.big_endian;

            start_offset += audio_offset;

            /* Vorbis is VBR so this is very approximate percent, meh */
            if (ww.truncated) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
            }

            break;
        }
#endif

        case DSP: {     /* Wii/3DS/WiiU */
            off_t wiih_offset;
            size_t wiih_size;

            //if (ww.fmt_size != 0x28 && ww.fmt_size != ?) goto fail; /* old, new */
            if (ww.bits_per_sample != 4) goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08; /* ww.block_align = 0x8 in older Wwise, samples per block in newer Wwise */

            /* find coef position */
            if (find_chunk(sf, 0x57696948,first_offset,0, &wiih_offset,&wiih_size, ww.big_endian, 0)) { /*"WiiH", older Wwise */
                vgmstream->num_samples = dsp_bytes_to_samples(ww.data_size, ww.channels);
                if (wiih_size != 0x2e * ww.channels) goto fail;

                if (is_dsp_full_interleave(sf, &ww, wiih_offset))
                    vgmstream->interleave_block_size = ww.data_size / 2;
            }
            else if (ww.extra_size == 0x0c + ww.channels * 0x2e) { /* newer Wwise */
                vgmstream->num_samples = read_32bit(ww.fmt_offset + 0x18, sf);
                wiih_offset = ww.fmt_offset + 0x1c;
                wiih_size = 0x2e * ww.channels;
            }
            else {
                goto fail;
            }

            if (ww.truncated) {
                ww.data_size = ww.file_size - ww.data_offset;
                vgmstream->num_samples = dsp_bytes_to_samples(ww.data_size, ww.channels);
            }

            /* for some reason all(?) DSP .wem do full loops (even mono/jingles/etc) but
             * several tracks do loop like this, so disable it for short-ish tracks */
            if (ww.loop_flag && vgmstream->loop_start_sample == 0 &&
                    vgmstream->loop_end_sample < 20*ww.sample_rate) { /* in seconds */
                vgmstream->loop_flag = 0;
            }

            dsp_read_coefs(vgmstream,sf,wiih_offset + 0x00, 0x2e, ww.big_endian);
            dsp_read_hist (vgmstream,sf,wiih_offset + 0x24, 0x2e, ww.big_endian);

            break;
        }

#ifdef VGM_USE_FFMPEG
        case XMA2: {    /* X360/XBone */
            uint8_t buf[0x100];
            int bytes;
            off_t xma2_offset;
            size_t xma2_size;

            /* endian check should be enough */
            //if (ww.fmt_size != ...) goto fail; /* XMA1 0x20, XMA2old: 0x34, XMA2new: 0x40, XMA2 Guitar Hero Live/padded: 0x64, etc */
            if (!ww.big_endian) goto fail; /* must be Wwise (real XMA are LE and parsed elsewhere) */

            if (find_chunk(sf, 0x584D4132,first_offset,0, &xma2_offset,&xma2_size, ww.big_endian, 0)) { /*"XMA2"*/ /* older Wwise */
                bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, xma2_offset, xma2_size, ww.data_size, sf);
            } else { /* newer Wwise */
                bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, ww.fmt_offset, ww.fmt_size, ww.data_size, sf, ww.big_endian);
            }

            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, ww.data_offset,ww.data_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ww.num_samples; /* set while parsing XMAWAVEFORMATs */

            /* Wwise loops are always pre-adjusted (old or new) and only num_samples is off */
            xma_fix_raw_samples(vgmstream, sf, ww.data_offset,ww.data_size, ww.chunk_offset, 1,0);

            /* "XMAc": rare Wwise extension, XMA2 physical loop regions (loop_start_b, loop_end_b, loop_subframe_data)
             * Can appear even in the file doesn't loop, maybe it's meant to be the playable physical region */
            //VGM_ASSERT(find_chunk(sf, 0x584D4163,first_offset,0, NULL,NULL, ww.big_endian, 0), "WWISE: XMAc chunk found\n");
            /* other chunks: "seek", regular XMA2 seek table */

            /* XMA is VBR so this is very approximate percent, meh */
            if (ww.truncated) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
            }

            break;
        }

        case XWMA: {    /* X360 */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[0x100];
            int bytes;

            if (ww.fmt_size != 0x18) goto fail;
            if (!ww.big_endian) goto fail; /* must be from Wwise X360 (PC LE XWMA is parsed elsewhere) */

            bytes = ffmpeg_make_riff_xwma(buf,0x100, ww.format, ww.data_size, vgmstream->channels, vgmstream->sample_rate, ww.average_bps, ww.block_align);
            ffmpeg_data = init_ffmpeg_header_offset(sf, buf,bytes, ww.data_offset,ww.data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;


            /* manually find total samples, why don't they put this in the header is beyond me */
            {
                ms_sample_data msd = {0};

                msd.channels = ww.channels;
                msd.data_offset = ww.data_offset;
                msd.data_size = ww.data_size;

                if (ww.format == 0x0162)
                    wmapro_get_samples(&msd, sf, ww.block_align, ww.sample_rate,0x00E0);
                else
                    wma_get_samples(&msd, sf, ww.block_align, ww.sample_rate,0x001F);

                vgmstream->num_samples = msd.num_samples;
                if (!vgmstream->num_samples)
                    vgmstream->num_samples = (int32_t)ffmpeg_data->totalSamples; /* very wrong, from avg-br */
                //num_samples seem to be found in the last "seek" table entry too, as: entry / channels / 2
            }

            break;
        }

        case AAC: {     /* iOS/Mac */
            ffmpeg_codec_data * ffmpeg_data = NULL;

            if (ww.fmt_size != 0x24) goto fail;
            if (ww.block_align != 0 || ww.bits_per_sample != 0) goto fail;

            /* extra: size 0x12, unknown values */

            ffmpeg_data = init_ffmpeg_offset(sf, ww.data_offset,ww.data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = (int32_t)ffmpeg_data->totalSamples;
            break;
        }

        case OPUSNX: {  /* Switch */
            size_t skip;

            /* values up to 0x14 seem fixed and similar to HEVAG's (block_align 0x02/04, bits_per_sample 0x10) */
            if (ww.fmt_size == 0x28) {
                size_t seek_size;

                vgmstream->num_samples += read_32bit(ww.fmt_offset + 0x18, sf);
                /* 0x1c: null? 0x20: data_size without seek_size */
                seek_size = read_32bit(ww.fmt_offset + 0x24, sf);

                start_offset += seek_size;
                ww.data_size -= seek_size;
            }
            else {
                goto fail;
            }

            skip = switch_opus_get_encoder_delay(start_offset, sf); /* should be 120 */

            /* OPUS is VBR so this is very approximate percent, meh */
            if (ww.truncated) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                ww.data_size = ww.file_size - start_offset;
            }

            vgmstream->codec_data = init_ffmpeg_switch_opus(sf, start_offset,ww.data_size, vgmstream->channels, skip, vgmstream->sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case OPUS: {     /* PC/mobile/etc, rare (most games still use Vorbis) [Girl Cafe Gun (Mobile)] */
            if (ww.block_align != 0 || ww.bits_per_sample != 0) goto fail;

            /* extra: size 0x12 */
            vgmstream->num_samples = read_32bit(ww.fmt_offset + 0x18, sf);
            /* 0x1c: stream size without OggS? */
            /* 0x20: full samples (without encoder delay) */

            /* OPUS is VBR so this is very approximate percent, meh */
            if (ww.truncated) {
                vgmstream->num_samples = (int32_t)(vgmstream->num_samples *
                        (double)(ww.file_size - start_offset) / (double)ww.data_size);
                ww.data_size = ww.file_size - start_offset;
            }

            vgmstream->codec_data = init_ffmpeg_offset(sf, ww.data_offset,ww.data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

#endif
        case HEVAG:     /* PSV */
            /* changed values, another bizarre Wwise quirk */
            //ww.block_align /* unknown (1ch=2, 2ch=4) */
            //ww.bits_per_sample; /* unknown (0x10) */
            //if (ww.bits_per_sample != 4) goto fail;

            if (ww.fmt_size != 0x18) goto fail;
            if (ww.big_endian) goto fail;

            /* extra_data: size 0x06, @0x00: samples per block (0x1c), @0x04: channel config */

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

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(ww.fmt_offset+0x18,sf);
            cfg.encoder_delay = read_32bit(ww.fmt_offset+0x20,sf);

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = read_32bit(ww.fmt_offset+0x1c,sf);
            break;
        }
#endif
        case PTADPCM: /* substitutes IMA as default ADPCM codec */
            if (ww.bits_per_sample != 4) goto fail;
            if (ww.block_align != 0x24 * ww.channels && ww.block_align != 0x104 * ww.channels) goto fail;

            vgmstream->coding_type = coding_PTADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = ww.block_align / ww.channels;
          //vgmstream->codec_endian = ww.big_endian; //?

            vgmstream->num_samples = ptadpcm_bytes_to_samples(ww.data_size, ww.channels, vgmstream->interleave_block_size);
            break;

        default:
            goto fail;
    }



    if ( !vgmstream_open_stream(vgmstream,sf,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static int is_dsp_full_interleave(STREAMFILE* sf, wwise_header* ww, off_t coef_offset) {
    /* older (only?) Wwise use full interleave for memory (in .bnk) files, but
     * detection from the .wem side is problematic [Punch Out!! (Wii)]
     * - truncated point to streams = normal
     * - .bnk would be memory banks = full
     * - otherwise small-ish sizes, stereo, with initial predictors for the
     *   second channel matching half size = full
     * some files aren't detectable like this though, when predictors are 0 
     * (but since memory wem aren't that used this shouldn't be too common) */

    if (ww->truncated)
        return 0;

    if (ww->channels == 1)
        return 0;

    if (check_extensions(sf,"bnk"))
        return 1;

    if (ww->data_size > 0x30000)
        return 0;

    {
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



/* VORBIS FORMAT RESEARCH */
/*
- old format
"fmt" size 0x28, extra size 0x16 / size 0x18, extra size 0x06
0x12 (2): flag? (00,10,18): not related to seek table, codebook type, chunk count, looping
0x14 (4): channel config
0x18-24 (16): ? (fixed: 0x01000000 00001000 800000AA 00389B71)  [removed when extra size is 0x06]

"vorb" size 0x34
0x00 (4): num_samples
0x04 (4): skip samples?
0x08 (4): ? (small if loop, 0 otherwise)
0x0c (4): data start offset after seek table+setup, or loop start when "smpl" is present
0x10 (4): ? (small, 0..~0x400)
0x14 (4): approximate data size without seek table? (almost setup+packets)
0x18 (4): setup_offset within data (0 = no seek table)
0x1c (4): audio_offset within data
0x20 (2): biggest packet size (not including header)?
0x22 (2): ? (small, N..~0x100) uLastGranuleExtra?
0x24 (4): ? (mid, 0~0x5000) dwDecodeAllocSize?
0x28 (4): ? (mid, 0~0x5000) dwDecodeX64AllocSize?
0x2c (4): parent bank/event id? uHashCodebook? (shared by several .wem a game, but not all need to share it)
0x30 (1): blocksize_1_exp (small)
0x31 (1): blocksize_0_exp (large)
0x32 (2): empty

"vorb" size 0x28 / 0x2c / 0x2a
0x00 (4): num_samples
0x04 (4): data start offset after seek table+setup, or loop start when "smpl" is present
0x08 (4): data end offset after seek table (setup+packets), or loop end when "smpl" is present
0x0c (2): ? (small, 0..~0x400) [(4) when size is 0x2C]
0x10 (4): setup_offset within data (0 = no seek table)
0x14 (4): audio_offset within data
0x18 (2): biggest packet size (not including header)?
0x1a (2): ? (small, N..~0x100) uLastGranuleExtra? [(4) when size is 0x2C]
0x1c (4): ? (mid, 0~0x5000) dwDecodeAllocSize?
0x20 (4): ? (mid, 0~0x5000) dwDecodeX64AllocSize?
0x24 (4): parent bank/event id? uHashCodebook? (shared by several .wem a game, but not all need to share it)
0x28 (1): blocksize_1_exp (small) [removed when size is 0x28]
0x29 (1): blocksize_0_exp (large) [removed when size is 0x28]

- new format:
"fmt" size 0x42, extra size 0x30
0x12 (2): flag? (00,10,18): not related to seek table, codebook type, chunk count, looping, etc
0x14 (4): channel config
0x18 (4): num_samples
0x1c (4): data start offset after seek table+setup, or loop start when "smpl" is present
0x20 (4): data end offset after seek table (setup+packets), or loop end when "smpl" is present
0x24 (2): ?1 (small, 0..~0x400)
0x26 (2): ?2 (small, N..~0x100): not related to seek table, codebook type, chunk count, looping, packet size, samples, etc
0x28 (4): setup offset within data (0 = no seek table)
0x2c (4): audio offset within data
0x30 (2): biggest packet size (not including header)
0x32 (2): (small, 0..~0x100) uLastGranuleExtra?
0x34 (4): ? (mid, 0~0x5000) dwDecodeAllocSize?
0x38 (4): ? (mid, 0~0x5000) dwDecodeX64AllocSize?
0x40 (1): blocksize_1_exp (small)
0x41 (1): blocksize_0_exp (large)

Wwise encoder options, unknown fields above may be reflect these:
 https://www.audiokinetic.com/library/edge/?source=Help&id=vorbis_encoder_parameters
*/
