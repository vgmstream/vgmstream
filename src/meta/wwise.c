#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"


/* Wwise uses a custom RIFF/RIFX header, non-standard enough that it's parsed it here.
 * There is some repetition from other metas, but not enough to bother me.
 *
 * Some info: https://www.audiokinetic.com/en/library/edge/
 */
typedef enum { PCM, IMA, VORBIS, DSP, XMA2, XWMA, AAC, HEVAG, ATRAC9 } wwise_codec;
typedef struct {
    int big_endian;
    size_t file_size;

    /* chunks references */
    off_t fmt_offset;
    size_t fmt_size;
    off_t data_offset;
    size_t data_size;

    /* standard fmt stuff */
    wwise_codec codec;
    int format;
    int channels;
    int sample_rate;
    int block_align;
    int bit_per_sample;
    size_t extra_size;

    int loop_flag;
    uint32_t num_samples;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;
} wwise_header;


/* Wwise - Audiokinetic Wwise (Wave Works Interactive Sound Engine) middleware */
VGMSTREAM * init_vgmstream_wwise(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    wwise_header ww;
    off_t start_offset, first_offset = 0xc;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* basic checks */
    /* .wem (Wwise Encoded Media) is "newer Wwise", used after the 2011.2 SDK (~july)
     * .wav (ex. Shadowrun X360) and .ogg (ex. KOF XII X360) are used only in older Wwise */
    if (!check_extensions(streamFile,"wem,wav,lwav,ogg,logg")) goto fail; /* .xma may be possible? */

    if ((read_32bitBE(0x00,streamFile) != 0x52494646) &&    /* "RIFF" (LE) */
        (read_32bitBE(0x00,streamFile) != 0x52494658))      /* "RIFX" (BE) */
        goto fail;
    if ((read_32bitBE(0x08,streamFile) != 0x57415645))      /* "WAVE" */
        goto fail;

    memset(&ww,0,sizeof(wwise_header));

    ww.big_endian = read_32bitBE(0x00,streamFile) == 0x52494658;/* RIFX */
    if (ww.big_endian) { /* Wwise honors machine's endianness (PC=RIFF, X360=RIFX --unlike XMA) */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    ww.file_size = streamFile->get_size(streamFile);

#if 0
    /* sometimes uses a RIFF size that doesn't count chunk/sizes, or just wrong...? */
    if (4+4+read_32bit(0x04,streamFile) != ww.file_size) {
        VGM_LOG("WWISE: bad riff size (real=0x%x vs riff=0x%x)\n", 4+4+read_32bit(0x04,streamFile), ww.file_size);
        goto fail;
    }
#endif


    /* parse WAVEFORMATEX (roughly spec-compliant but some massaging is needed) */
    {
        off_t loop_offset;
        size_t loop_size;

        /* find basic chunks */
        if (!find_chunk(streamFile, 0x666d7420,first_offset,0, &ww.fmt_offset,&ww.fmt_size, ww.big_endian, 0)) goto fail; /*"fmt "*/
        if (!find_chunk(streamFile, 0x64617461,first_offset,0, &ww.data_offset,&ww.data_size, ww.big_endian, 0)) goto fail; /*"data"*/

        if (ww.data_size > ww.file_size) {
            VGM_LOG("WWISE: bad data size (real=0x%x > riff=0x%x)\n", ww.data_size, ww.file_size);
            goto fail;
        }

        /* base fmt */
        if (ww.fmt_size < 0x12) goto fail;
        ww.format           = (uint16_t)read_16bit(ww.fmt_offset+0x00,streamFile);
        ww.channels         = read_16bit(ww.fmt_offset+0x02,streamFile);
        ww.sample_rate      = read_32bit(ww.fmt_offset+0x04,streamFile);
        /* 0x08: average samples per second */
        ww.block_align      = (uint16_t)read_16bit(ww.fmt_offset+0x0c,streamFile);
        ww.bit_per_sample   = (uint16_t)read_16bit(ww.fmt_offset+0x0e,streamFile);
        if (ww.fmt_size > 0x10 && ww.format != 0x0165 && ww.format != 0x0166) /* ignore XMAWAVEFORMAT */
            ww.extra_size   = (uint16_t)read_16bit(ww.fmt_offset+0x10,streamFile);
#if 0
        /* channel bitmask, see AkSpeakerConfig.h (ex. 1ch uses FRONT_CENTER 0x4, 2ch FRONT_LEFT 0x1 | FRONT_RIGHT 0x2) */
        if (ww.extra_size >= 6)
            ww.channel_config = read_32bit(ww.fmt_offset+0x14,streamFile);
#endif

        /* find loop info (both used in early and late Wwise and seem to follow the spec) */
        if (find_chunk(streamFile, 0x736D706C,first_offset,0, &loop_offset,&loop_size, ww.big_endian, 0)) { /*"smpl"*/
            if (loop_size >= 0x34
                    && read_32bit(loop_offset+0x1c, streamFile)==1        /*loop count*/
                    && read_32bit(loop_offset+0x24+4, streamFile)==0) {
                ww.loop_flag = 1;
                ww.loop_start_sample = read_32bit(loop_offset+0x24+0x8, streamFile);
                ww.loop_end_sample   = read_32bit(loop_offset+0x24+0xc,streamFile);
                //todo fix repeat looping
            }
        } else if (find_chunk(streamFile, 0x4C495354,first_offset,0, &loop_offset,&loop_size, ww.big_endian, 0)) { /*"LIST"*/
            //todo parse "adtl" (does it ever contain loop info in Wwise?)
        }

        /* other Wwise specific: */
        //"JUNK": optional padding so that raw data starts in an offset multiple of 0x10 (0-size JUNK exists)
        //"akd ": unknown (IMA/PCM; "audiokinetic data"?)
    }

    /* format to codec */
    switch(ww.format) {
        case 0x0001: ww.codec = PCM; break; /* older Wwise */
        case 0x0002: ww.codec = IMA; break; /* newer Wwise (conflicts with MSADPCM) */
        case 0x0011: ww.codec = IMA; break; /* older Wwise */
        case 0x0069: ww.codec = IMA; break; /* older Wwise */
        case 0x0165: ww.codec = XMA2; break;
        case 0x0166: ww.codec = XMA2; break;
        case 0xAAC0: ww.codec = AAC; break;
        case 0xFFF0: ww.codec = DSP; break;
        case 0xFFFB: ww.codec = HEVAG; break;
        case 0xFFFC: ww.codec = ATRAC9; break;
        case 0xFFFE: ww.codec = PCM; break; /* newer Wwise ("PCM for Wwise Authoring") (conflicts with WAVEFORMATEXTENSIBLE) */
        case 0xFFFF: ww.codec = VORBIS; break;
        default:
            VGM_LOG("WWISE: unknown codec 0x%x \n", ww.format);
            goto fail;
    }
    /* fix for newer Wwise DSP with coefs: Epic Mickey 2 (Wii), Batman Arkham Origins Blackgate (3DS) */
    if (ww.format == 0x0002 && ww.extra_size == 0x0c + ww.channels * 0x2e) {
        ww.codec = DSP;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ww.channels,ww.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ww.sample_rate;
    vgmstream->num_samples = ww.num_samples;
    vgmstream->loop_start_sample = ww.loop_start_sample;
    vgmstream->loop_end_sample = ww.loop_end_sample;
    vgmstream->meta_type = meta_WWISE_RIFF;

    start_offset = ww.data_offset;

    switch(ww.codec) {
        case PCM: /* common */
            /* normally riff.c has priority but it's needed when .wem is used */
            if (ww.bit_per_sample != 16) goto fail;

            vgmstream->coding_type = (ww.big_endian ? coding_PCM16BE : coding_PCM16LE);
            vgmstream->layout_type = ww.channels > 1 ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = ww.data_size / ww.channels / (ww.bit_per_sample/8);
            break;

        case IMA: /* common */
            /* slightly modified MS-IMA.
             * Original research by hcs in ima_rejigger (https://github.com/hcs64/vgm_ripping/tree/master/demux/ima_rejigger5) */
#if 0
            if (ww.bit_per_sample != 4) goto fail;
            vgmstream->coding_type = coding_WWISE_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = ww.block_align;

            vgmstream->num_samples = (ww.data_size / ww.block_align) * (ww.block_align - 4 * vgmstream->channels) * 2 /vgmstream->channels;
            break;
#endif
            VGM_LOG("WWISE: IMA found (unsupported)\n");
            goto fail;

#ifdef VGM_USE_VORBIS
        case VORBIS: {  /* common */
            /* Wwise uses custom Vorbis, which changed over time (config must be detected to pass to the decoder).
             * Original research by hcs in ww2ogg (https://github.com/hcs64/ww2ogg) */
#if 0
            off_t vorb_offset;
            size_t vorb_size;

            int packet_header_type = 0; /* 1 = size 8 (4-granule + 4-size), 2 = size 6 (4-granule + 2-size) or 3 = size 2 (2-size) */
            int packet_type = 0; /* 1 = standard, 2 = modified vorbis packets */
            int setup_type = 0; /* 1: triad, 2 = inline codebooks, 3 = external codebooks, 4 = external aoTuV codebooks */
            int blocksize_0_pow = 0, blocksize_1_pow = 0;

            if (ww.block_align != 0 || ww.bit_per_sample != 0) goto fail;

            /* autodetect format */
            if (find_chunk(streamFile, 0x766F7262,first_offset,0, &vorb_offset,&vorb_size, ww.big_endian, 0)) { /*"vorb"*/
                /* older Wwise (~2011) */
                switch (vorb_size) {
                    case 0x28:
                    case 0x2A:
                    case 0x2C:
                    case 0x32:
                    case 0x34:
                    default: goto fail;
                }

            }
            else {
                /* newer Wwise (~2012+) */
                if (ww.extra_size != 0x30) goto fail; //todo some 0x2a exist

                packet_header_type = 3; /* size 2 */
                packet_type = 1; //todo mod packets false on certain configs
                setup_type = 4; /* aoTuV */

                /* 0x12 (2): unk (00,10,18) not related to seek table*/
                /* 0x14 (4): channel config */
                vgmstream->num_samples = read_32bit(ww.fmt_offset + 0x18, streamFile);
                /* 0x20 (4): config, 0xcb/0xd9/etc */
                /* 0x24 (4): ? samples? */
                /* 0x28 (4): seek table size (setup packet offset within data) */
                /* 0x2c (4): setup size (first audio packet offset within data) */
                /* 0x30 (2): ? */
                /* 0x32 (2): ? */
                /* 0x34 (4): ? */
                /* 0x38 (4): ? */
                /* 0x3c (4): uid */ //todo same as external crc32?
                blocksize_0_pow = read_8bit(ww.fmt_offset + 0x40, streamFile);
                blocksize_1_pow = read_8bit(ww.fmt_offset + 0x41, streamFile);

                goto fail;
            }


            vgmstream->codec_data = init_wwise_vorbis_codec_data(streamFile, start_offset, vgmstream->channels, vgmstream->sample_rate);//pass endianness too
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_wwise_vorbis;
            vgmstream->layout_type = layout_none;
            break;
#endif
            VGM_LOG("WWISE: VORBIS found (unsupported)\n");
            goto fail;
        }
#endif

        case DSP: {     /* Wii/3DS/WiiU */
            off_t wiih_offset;
            size_t wiih_size;
            int i;

            if (ww.bit_per_sample != 4) goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8; /* ww.block_align = 0x8 in older Wwise, samples per block in newer Wwise */

            /* find coef position */
            if (find_chunk(streamFile, 0x57696948,first_offset,0, &wiih_offset,&wiih_size, ww.big_endian, 0)) { /*"WiiH"*/ /* older Wwise */
                vgmstream->num_samples = ww.data_size / ww.channels / 8 * 14;
                if (wiih_size != 0x2e * ww.channels) goto fail;
            }
            else if (ww.extra_size == 0x0c + ww.channels * 0x2e) { /* newer Wwise */
                vgmstream->num_samples = read_32bit(ww.fmt_offset + 0x18, streamFile);
                wiih_offset = ww.fmt_offset + 0x1c;
                wiih_size = 0x2e * ww.channels;
            }
            else {
                goto fail;
            }

            /* get coefs and default history */
            dsp_read_coefs(vgmstream,streamFile,wiih_offset, 0x2e, ww.big_endian);
            for (i=0; i < ww.channels; i++) {
                vgmstream->ch[i].adpcm_history1_16 = read_16bitBE(wiih_offset + i * 0x2e + 0x24,streamFile);
                vgmstream->ch[i].adpcm_history2_16 = read_16bitBE(wiih_offset + i * 0x2e + 0x26,streamFile);
            }

            break;
        }

#ifdef VGM_USE_FFMPEG
        case XMA2: {    /* X360/XBone */
            //chunks:
            //"XMA2", "seek": same as the official ones
            //"XMAc": Wwise extension, XMA2 physical loop regions (loop_start_b, loop_end_b, loop_subframe_data)

            VGM_LOG("WWISE: XMA2 found (unsupported)\n");
            goto fail;
        }

        case XWMA:      /* X360 */
            VGM_LOG("WWISE: XWMA found (unsupported)\n");
            goto fail;

        case AAC: {     /* iOS/Mac */
            ffmpeg_codec_data * ffmpeg_data = NULL;
            if (ww.block_align != 0 || ww.bit_per_sample != 0) goto fail;

            /* extra: size 0x12, unknown values */

            ffmpeg_data = init_ffmpeg_offset(streamFile, ww.data_offset,ww.data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ffmpeg_data->totalSamples;
            break;
        }
#endif
        case HEVAG:     /* PSV */
            /* changed values, another bizarre Wwise quirk */
            //ww.block_align /* unknown (1ch=2, 2ch=4) */
            //ww.bit_per_sample; /* probably interleave (0x10) */
            //if (ww.bit_per_sample != 4) goto fail;

            if (ww.big_endian) goto fail;

            /* extra_data: size 0x06, @0x00: samples per block (28), @0x04: channel config */

            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ww.data_size * 28 / 16 / ww.channels;
            break;

        case ATRAC9:    /* PSV/PS4 */
            VGM_LOG("WWISE: ATRAC9 found (unsupported)\n");
            goto fail;

        default:
            goto fail;
    }


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
