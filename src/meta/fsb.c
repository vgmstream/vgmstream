#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "fsb_interleave_streamfile.h"


/* ************************************************************************************************
 * FSB defines, copied from the public spec (https://www.fmod.org/questions/question/forum-4928/)
 * for reference. The format is mostly compatible for FSB1/2/3/4, but not FSB5.
 * ************************************************************************************************ */
/* These flags are used for FMOD_FSB_HEADER::mode */
#define FMOD_FSB_SOURCE_FORMAT         0x00000001  /* all samples stored in their original compressed format */
#define FMOD_FSB_SOURCE_BASICHEADERS   0x00000002  /* samples should use the basic header structure */
#define FMOD_FSB_SOURCE_ENCRYPTED      0x00000004  /* all sample data is encrypted */
#define FMOD_FSB_SOURCE_BIGENDIANPCM   0x00000008  /* pcm samples have been written out in big-endian format */
#define FMOD_FSB_SOURCE_NOTINTERLEAVED 0x00000010  /* Sample data is not interleaved. */
#define FMOD_FSB_SOURCE_MPEG_PADDED    0x00000020  /* Mpeg frames are now rounded up to the nearest 2 bytes for normal sounds, or 16 bytes for multichannel. */
#define FMOD_FSB_SOURCE_MPEG_PADDED4   0x00000040  /* Mpeg frames are now rounded up to the nearest 4 bytes for normal sounds, or 16 bytes for multichannel. */

/* These flags are used for FMOD_FSB_HEADER::version */
#define FMOD_FSB_VERSION_3_0         0x00030000  /* FSB version 3.0 */
#define FMOD_FSB_VERSION_3_1         0x00030001  /* FSB version 3.1 */
#define FMOD_FSB_VERSION_4_0         0x00040000  /* FSB version 4.0 */

/* FMOD 3 defines.  These flags are used for FMOD_FSB_SAMPLE_HEADER::mode */
#define FSOUND_LOOP_OFF              0x00000001  /* For non looping samples. */
#define FSOUND_LOOP_NORMAL           0x00000002  /* For forward looping samples. */
#define FSOUND_LOOP_BIDI             0x00000004  /* For bidirectional looping samples.  (no effect if in hardware). */
#define FSOUND_8BITS                 0x00000008  /* For 8 bit samples. */
#define FSOUND_16BITS                0x00000010  /* For 16 bit samples. */
#define FSOUND_MONO                  0x00000020  /* For mono samples. */
#define FSOUND_STEREO                0x00000040  /* For stereo samples. */
#define FSOUND_UNSIGNED              0x00000080  /* For user created source data containing unsigned samples. */
#define FSOUND_SIGNED                0x00000100  /* For user created source data containing signed data. */
#define FSOUND_MPEG                  0x00000200  /* For MPEG layer 2/3 data. */
#define FSOUND_CHANNELMODE_ALLMONO   0x00000400  /* Sample is a collection of mono channels. */
#define FSOUND_CHANNELMODE_ALLSTEREO 0x00000800  /* Sample is a collection of stereo channel pairs */
#define FSOUND_HW3D                  0x00001000  /* Attempts to make samples use 3d hardware acceleration. (if the card supports it) */
#define FSOUND_2D                    0x00002000  /* Tells software (not hardware) based sample not to be included in 3d processing. */
#define FSOUND_SYNCPOINTS_NONAMES    0x00004000  /* Specifies that syncpoints are present with no names */
#define FSOUND_DUPLICATE             0x00008000  /* This subsound is a duplicate of the previous one i.e. it uses the same sample data but w/different mode bits */
#define FSOUND_CHANNELMODE_PROTOOLS  0x00010000  /* Sample is 6ch and uses L C R LS RS LFE standard. */
#define FSOUND_MPEGACCURATE          0x00020000  /* For FSOUND_Stream_Open - for accurate FSOUND_Stream_GetLengthMs/FSOUND_Stream_SetTime.  WARNING, see FSOUND_Stream_Open for inital opening time performance issues. */
#define FSOUND_HW2D                  0x00080000  /* 2D hardware sounds.  allows hardware specific effects */
#define FSOUND_3D                    0x00100000  /* 3D software sounds */
#define FSOUND_32BITS                0x00200000  /* For 32 bit (float) samples. */
#define FSOUND_IMAADPCM              0x00400000  /* Contents are stored compressed as IMA ADPCM */
#define FSOUND_VAG                   0x00800000  /* For PS2 only - Contents are compressed as Sony VAG format */
#define FSOUND_XMA                   0x01000000  /* For Xbox360 only - Contents are compressed as XMA format */
#define FSOUND_GCADPCM               0x02000000  /* For Gamecube only - Contents are compressed as Gamecube DSP-ADPCM format */
#define FSOUND_MULTICHANNEL          0x04000000  /* For PS2 and Gamecube only - Contents are interleaved into a multi-channel (more than stereo) format */
#define FSOUND_OGG                   0x08000000  /* For vorbis encoded ogg data */
#define FSOUND_CELT                  0x08000000  /* For vorbis encoded ogg data */
#define FSOUND_MPEG_LAYER3           0x10000000  /* Data is in MP3 format. */
#define FSOUND_MPEG_LAYER2           0x00040000  /* Data is in MP2 format. */
#define FSOUND_LOADMEMORYIOP         0x20000000  /* For PS2 only - &quot;name&quot; will be interpreted as a pointer to data for streaming and samples.  The address provided will be an IOP address */
#define FSOUND_IMAADPCMSTEREO        0x20000000  /* Signify IMA ADPCM is actually stereo not two interleaved mono */
#define FSOUND_IGNORETAGS            0x40000000  /* Skips id3v2 etc tag checks when opening a stream, to reduce seek/read overhead when opening files (helps with CD performance) */
#define FSOUND_SYNCPOINTS            0x80000000  /* Specifies that syncpoints are present */

/* These flags are used for FMOD_FSB_SAMPLE_HEADER::mode */
#define FSOUND_CHANNELMODE_MASK      (FSOUND_CHANNELMODE_ALLMONO | FSOUND_CHANNELMODE_ALLSTEREO | FSOUND_CHANNELMODE_PROTOOLS)
#define FSOUND_CHANNELMODE_DEFAULT   0x00000000  /* Determine channel assignment automatically from channel count. */
#define FSOUND_CHANNELMODE_RESERVED  0x00000C00
#define FSOUND_NORMAL                (FSOUND_16BITS | FSOUND_SIGNED | FSOUND_MONO)
#define FSB_SAMPLE_DATA_ALIGN        32


/* simplified struct based on the original definitions */
typedef enum { MPEG, IMA, PSX, XMA2, DSP, CELT, PCM8, PCM16 } fsb_codec_t;
typedef struct {
    /* main header */
    uint32_t id;
    int32_t  total_subsongs;
    uint32_t sample_headers_size; /* all of them including extended information */
    uint32_t sample_data_size;
    uint32_t version; /* extended fsb version (in FSB 3/3.1/4) */
    uint32_t flags; /* flags common to all streams (in FSB 3/3.1/4)*/
    /* sample header */
    uint32_t num_samples;
    uint32_t stream_size;
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t mode;
    int32_t  sample_rate;
    uint16_t channels;
    /* extra */
    uint32_t base_header_size;
    uint32_t sample_header_min;
    off_t extradata_offset;
    off_t first_extradata_offset;

    meta_t meta_type;
    off_t name_offset;
    size_t name_size;

    int loop_flag;

    off_t stream_offset;

    fsb_codec_t codec;
} fsb_header;

/* ********************************************************************************** */

#ifdef VGM_USE_CELT
static layered_layout_data* build_layered_fsb_celt(STREAMFILE *streamFile, fsb_header* fsb, celt_lib_t version);
#endif

/* FSB1~4 - from games using FMOD audio middleware */
VGMSTREAM * init_vgmstream_fsb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int target_subsong = streamFile->stream_index;
    fsb_header fsb = {0};


    /* checks
     * .fsb: standard
     * .bnk: Hard Corps Uprising (PS3) */
    if ( !check_extensions(streamFile, "fsb,bnk") )
        goto fail;

    /* check header */
    fsb.id = read_32bitBE(0x00,streamFile);
    if (fsb.id == 0x46534231) { /* "FSB1" (somewhat different from other fsbs) */
        fsb.meta_type = meta_FSB1;
        fsb.base_header_size = 0x10;
        fsb.sample_header_min = 0x40;

        /* main header */
        fsb.total_subsongs = read_32bitLE(0x04,streamFile);
        fsb.sample_data_size = read_32bitLE(0x08,streamFile);
        fsb.sample_headers_size = 0x40;
        fsb.version = 0;
        fsb.flags = 0;

        if (fsb.total_subsongs > 1) goto fail;

        /* sample header (first stream only, not sure if there are multi-FSB1) */
        {
            off_t header_offset = fsb.base_header_size;

            fsb.name_offset = header_offset;
            fsb.name_size   = 0x20;
            fsb.num_samples = read_32bitLE(header_offset+0x20,streamFile);
            fsb.stream_size = read_32bitLE(header_offset+0x24,streamFile);
            fsb.sample_rate = read_32bitLE(header_offset+0x28,streamFile);
            /* 0x2c:?  0x2e:?  0x30:?  0x32:? */
            fsb.mode        = read_32bitLE(header_offset+0x34,streamFile);
            fsb.loop_start  = read_32bitLE(header_offset+0x38,streamFile);
            fsb.loop_end    = read_32bitLE(header_offset+0x3c,streamFile);

            fsb.channels = (fsb.mode & FSOUND_STEREO) ? 2 : 1;
            if (fsb.loop_end > fsb.num_samples) /* this seems common... */
                fsb.num_samples = fsb.loop_end;

            /* DSP coefs, seek tables, etc */
            fsb.extradata_offset = header_offset+fsb.sample_header_min;

            fsb.stream_offset = fsb.base_header_size + fsb.sample_headers_size;
        }
    }
    else { /* other FSBs (common/extended format) */
        if (fsb.id == 0x46534232) { /* "FSB2" */
            fsb.meta_type = meta_FSB2;
            fsb.base_header_size  = 0x10;
            fsb.sample_header_min = 0x40; /* guessed */
        } else if (fsb.id == 0x46534233) { /* "FSB3" */
            fsb.meta_type = meta_FSB3;
            fsb.base_header_size  = 0x18;
            fsb.sample_header_min = 0x40;
        } else if (fsb.id == 0x46534234) { /* "FSB4" */
            fsb.meta_type = meta_FSB4;
            fsb.base_header_size  = 0x30;
            fsb.sample_header_min = 0x50;
        } else {
            goto fail;
        }

        /* main header */
        fsb.total_subsongs = read_32bitLE(0x04,streamFile);
        fsb.sample_headers_size = read_32bitLE(0x08,streamFile);
        fsb.sample_data_size = read_32bitLE(0x0c,streamFile);
        if (fsb.base_header_size > 0x10) {
            fsb.version = read_32bitLE(0x10,streamFile);
            fsb.flags   = read_32bitLE(0x14,streamFile);
            /* FSB4: 0x18(8):hash  0x20(10):guid */
        } else {
            fsb.version = 0;
            fsb.flags   = 0;
        }

        if (fsb.version == FMOD_FSB_VERSION_3_1) {
            fsb.sample_header_min = 0x50;
        } else if (fsb.version != 0 /* FSB2 */
                && fsb.version != FMOD_FSB_VERSION_3_0
                && fsb.version != FMOD_FSB_VERSION_4_0) {
            goto fail;
        }

        if (fsb.sample_headers_size < fsb.sample_header_min) goto fail;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > fsb.total_subsongs || fsb.total_subsongs < 1) goto fail;

        /* sample header (N-stream) */
        {
            int i;
            off_t header_offset = fsb.base_header_size;
            off_t data_offset = fsb.base_header_size + fsb.sample_headers_size;

            /* find target_stream header (variable sized) */
            for (i = 0; i < fsb.total_subsongs; i++) {
                size_t stream_header_size;

                if ((fsb.flags & FMOD_FSB_SOURCE_BASICHEADERS) && i > 0) {
                    /* miniheader, all subsongs reuse first header [rare, ex. Biker Mice from Mars (PS2)] */
                    stream_header_size = 0x08;
                    fsb.num_samples = read_32bitLE(header_offset+0x00,streamFile);
                    fsb.stream_size = read_32bitLE(header_offset+0x04,streamFile);
                    fsb.loop_start = 0;
                    fsb.loop_end = 0;
                }
                else {
                    /* subsong header for normal files */
                    stream_header_size = (uint16_t)read_16bitLE(header_offset+0x00,streamFile);
                    fsb.name_offset = header_offset+0x02;
                    fsb.name_size   = 0x20-0x02;
                    fsb.num_samples = read_32bitLE(header_offset+0x20,streamFile);
                    fsb.stream_size = read_32bitLE(header_offset+0x24,streamFile);
                    fsb.loop_start  = read_32bitLE(header_offset+0x28,streamFile);
                    fsb.loop_end    = read_32bitLE(header_offset+0x2c,streamFile);
                    fsb.mode        = read_32bitLE(header_offset+0x30,streamFile);
                    fsb.sample_rate = read_32bitLE(header_offset+0x34,streamFile);
                    /* 0x38: defvol, 0x3a: defpan, 0x3c: defpri */
                    fsb.channels = read_16bitLE(header_offset+0x3e,streamFile);
                    /* FSB3.1/4:
                     *   0x40: mindistance, 0x44: maxdistance, 0x48: varfreq/size_32bits
                     *   0x4c: varvol, 0x4e: fsb.varpan */

                    /* DSP coefs, seek tables, etc */
                    if (stream_header_size > fsb.sample_header_min) {
                        fsb.extradata_offset = header_offset+fsb.sample_header_min;
                        if (fsb.first_extradata_offset == 0)
                            fsb.first_extradata_offset = fsb.extradata_offset;
                    }
                }

                if (i+1 == target_subsong) /* final data_offset found */
                    break;

                header_offset += stream_header_size;
                data_offset += fsb.stream_size; /* there is no offset so manually count */

                /* some subsongs offsets need padding (most FSOUND_IMAADPCM, few MPEG too [Hard Reset (PC) subsong 5])
                 * other codecs may set PADDED4 (ex. XMA) but don't seem to need it and work fine */
                if (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED4) {
                    if (data_offset % 0x20)
                        data_offset += 0x20 - (data_offset % 0x20);
                }
            }
            if (i > fsb.total_subsongs)
                goto fail; /* not found */

            fsb.stream_offset = data_offset;
        }
    }


    /* XOR encryption for some FSB4, though the flag is only seen after decrypting */
    //;VGM_ASSERT(fsb.flags & FMOD_FSB_SOURCE_ENCRYPTED, "FSB ENCRYPTED found\n");

    /* sometimes there is garbage at the end or missing bytes due to improper ripping */
    VGM_ASSERT(fsb.base_header_size + fsb.sample_headers_size + fsb.sample_data_size != streamFile->get_size(streamFile),
               "FSB wrong head/data_size found (expected 0x%x vs 0x%x)\n",
               fsb.base_header_size + fsb.sample_headers_size + fsb.sample_data_size, streamFile->get_size(streamFile));

    /* Loops unless disabled. FMOD default seems to be full loops (0/num_samples-1) without flags, for repeating tracks
     * that should loop and jingles/sfx that shouldn't. We'll try to disable looping if it looks jingly enough. */
    fsb.loop_flag = !(fsb.mode & FSOUND_LOOP_OFF);
    if(!(fsb.mode & FSOUND_LOOP_NORMAL)                             /* rarely set */
            && fsb.loop_start+fsb.loop_end+1 == fsb.num_samples     /* full loop */
            && fsb.num_samples < 20*fsb.sample_rate)                /* in seconds (lame but no other way to know) */
        fsb.loop_flag = 0;

    /* ping-pong looping = no looping? (forward > reverse > forward) [ex. Biker Mice from Mars (PS2)] */
    VGM_ASSERT(fsb.mode & FSOUND_LOOP_BIDI, "FSB BIDI looping found\n");

    /* convert to clean some code */
    if      (fsb.mode & FSOUND_MPEG)        fsb.codec = MPEG;
    else if (fsb.mode & FSOUND_IMAADPCM)    fsb.codec = IMA;
    else if (fsb.mode & FSOUND_VAG)         fsb.codec = PSX;
    else if (fsb.mode & FSOUND_XMA)         fsb.codec = XMA2;
    else if (fsb.mode & FSOUND_GCADPCM)     fsb.codec = DSP;
    else if (fsb.mode & FSOUND_CELT)        fsb.codec = CELT;
    else if (fsb.mode & FSOUND_8BITS)       fsb.codec = PCM8;
    else                                    fsb.codec = PCM16;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsb.channels,fsb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fsb.sample_rate;
    vgmstream->num_samples = fsb.num_samples;
    vgmstream->loop_start_sample = fsb.loop_start;
    vgmstream->loop_end_sample = fsb.loop_end;
    vgmstream->num_streams = fsb.total_subsongs;
    vgmstream->stream_size = fsb.stream_size;
    vgmstream->meta_type = fsb.meta_type;
    if (fsb.name_offset)
        read_string(vgmstream->stream_name,fsb.name_size+1, fsb.name_offset,streamFile);

    switch(fsb.codec) {
#ifdef VGM_USE_MPEG
        case MPEG: { /* FSB4: Shatter (PS3), Way of the Samurai 3/4 (PS3) */
            mpeg_custom_config cfg = {0};

            cfg.fsb_padding = (vgmstream->channels > 2 ? 16 :
                (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED4 ? 4 :
                (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED ? 2 : 0)));

            vgmstream->codec_data = init_mpeg_custom(streamFile, fsb.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            //VGM_ASSERT(fsb.mode & FSOUND_MPEG_LAYER2, "FSB FSOUND_MPEG_LAYER2 found\n");/* not always set anyway */
            VGM_ASSERT(fsb.mode & FSOUND_IGNORETAGS, "FSB FSOUND_IGNORETAGS found\n"); /* not seen */
            break;
        }
#endif

        case IMA: /* FSB3: Bioshock (PC), FSB4: Blade Kitten (PC) */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            /* "interleaved header" IMA, only used with >2ch (ex. Blade Kitten 6ch)
             * or (seemingly) when flag is used (ex. Dead to Rights 2 (Xbox) 2ch in FSB3.1) */
            if (vgmstream->channels > 2 || (fsb.mode & FSOUND_MULTICHANNEL))
                vgmstream->coding_type = coding_FSB_IMA;

            /* FSOUND_IMAADPCMSTEREO is "noninterleaved, true stereo IMA", but doesn't seem to be any different
             * (found in FSB4: Shatter, Blade Kitten (PC), Hard Corps: Uprising (PS3)) */
            break;

        case PSX:  /* FSB1: Jurassic Park Operation Genesis (PS2), FSB4: Spider Man Web of Shadows (PSP) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (fsb.flags & FMOD_FSB_SOURCE_NOTINTERLEAVED) {
                vgmstream->interleave_block_size = fsb.stream_size / fsb.channels;
            }
            else {
                vgmstream->interleave_block_size = 0x10;
            }
            break;

#ifdef VGM_USE_FFMPEG
        case XMA2: { /* FSB3: The Bourne Conspiracy 2008 (X360), FSB4: Armored Core V (X360), Hard Corps (X360) */
            uint8_t buf[0x100];
            size_t bytes, block_size, block_count;

            block_size = 0x8000; /* FSB default */
            block_count = fsb.stream_size / block_size; /* not accurate but not needed (custom_data_offset+0x14 -1?) */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, fsb.num_samples, fsb.stream_size, fsb.channels, fsb.sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, fsb.stream_offset,fsb.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case DSP: /* FSB3: Metroid Prime 3 (GC), FSB4: de Blob (Wii) */
            if (fsb.flags & FMOD_FSB_SOURCE_NOTINTERLEAVED) { /* [de Blob (Wii) sfx)] */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = fsb.stream_size / fsb.channels;
            }
            else {
                vgmstream->coding_type = coding_NGC_DSP_subint;
                vgmstream->layout_type = layout_none;
                vgmstream->interleave_block_size = 0x2;
            }
            dsp_read_coefs_be(vgmstream, streamFile, fsb.extradata_offset, 0x2e);
            break;

#ifdef VGM_USE_CELT
        case CELT: { /* FSB4: War Thunder (PC), The Witcher 2 (PC), Vessel (PC) */
            celt_lib_t version;

            /* get libcelt version (set in the first subsong only, but try all extradata just in case) */
            if (fsb.first_extradata_offset || fsb.extradata_offset) {
                uint32_t lib = fsb.first_extradata_offset ?
                        (uint32_t)read_32bitLE(fsb.first_extradata_offset, streamFile) :
                        (uint32_t)read_32bitLE(fsb.extradata_offset, streamFile);;
                switch(lib) {
                    case 0x80000009: version = CELT_0_06_1; break; /* War Thunder (PC) */
                    case 0x80000010: version = CELT_0_11_0; break; /* Vessel (PC) */
                    default: VGM_LOG("FSB: unknown CELT lib 0x%x\n", lib); goto fail;
                }
            }
            else {
                /* split FSBs? try to guess from observed bitstreams */
                uint16_t frame = (uint16_t)read_16bitBE(fsb.stream_offset+0x04+0x04,streamFile);
                if ((frame & 0xF000) == 0x6000 || frame == 0xFFFE) {
                    version = CELT_0_11_0;
                } else {
                    version = CELT_0_06_1;
                }
            }

            if (fsb.channels > 2) { /* multistreams */
                vgmstream->layout_data = build_layered_fsb_celt(streamFile, &fsb, version);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_layered;
            }
            else {
                vgmstream->codec_data = init_celt_fsb(vgmstream->channels, version);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_none;
            }

            break;
        }
#endif

        case PCM8: /* assumed, no games known */
            vgmstream->coding_type = (fsb.mode & FSOUND_UNSIGNED) ? coding_PCM8_U : coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x1;
            break;

        case PCM16: /* (PCM16) FSB4: Rocket Knight (PC), Another Century's Episode R (PS3), Toy Story 3 (Wii) */
            vgmstream->coding_type = (fsb.flags & FMOD_FSB_SOURCE_BIGENDIANPCM) ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;

            /* sometimes FSOUND_MONO/FSOUND_STEREO is not set (ex. Dead Space iOS),
             * or only STEREO/MONO but not FSOUND_8BITS/FSOUND_16BITS is set */
            break;

        default:
            goto fail;
    }


    if ( !vgmstream_open_stream(vgmstream, streamFile, fsb.stream_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_CELT
static layered_layout_data* build_layered_fsb_celt(STREAMFILE *streamFile, fsb_header* fsb, celt_lib_t version) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    int i, layers = (fsb->channels+1) / 2;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (1/2ch CELT streams: 2ch+2ch..+1ch or 2ch+2ch..+2ch) */
    for (i = 0; i < layers; i++) {
        int layer_channels = (i+1 == layers && fsb->channels % 2 == 1)
                ? 1 : 2; /* last layer can be 1/2ch */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, fsb->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = fsb->sample_rate;
        data->layers[i]->num_samples = fsb->num_samples;
        data->layers[i]->loop_start_sample = fsb->loop_start;
        data->layers[i]->loop_end_sample = fsb->loop_end;

        data->layers[i]->codec_data = init_celt_fsb(layer_channels, version);
        if (!data->layers[i]->codec_data) goto fail;
        data->layers[i]->coding_type = coding_CELT_FSB;
        data->layers[i]->layout_type = layout_none;

        temp_streamFile = setup_fsb_interleave_streamfile(streamFile, fsb->stream_offset, fsb->stream_size, layers, i, FSB_INT_CELT);
        if (!temp_streamFile) goto fail;

        if ( !vgmstream_open_stream(data->layers[i], temp_streamFile, 0x00) ) {
            goto fail;
        }
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    close_streamfile(temp_streamFile);
    return data;

fail:
    close_streamfile(temp_streamFile);
    free_layout_layered(data);
    return NULL;
}
#endif

/* ****************************************** */

static STREAMFILE* setup_fsb4_wav_streamfile(STREAMFILE *streamfile, off_t subfile_offset, size_t subfile_size);

/* FSB4 with "\0WAV" Header, found in Deadly Creatures (Wii).
 * Has a 0x10 BE header that holds the filesize (unsure if this is from a proper rip). */
VGMSTREAM * init_vgmstream_fsb4_wav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *test_streamFile = NULL;
    off_t subfile_start = 0x10;
    size_t subfile_size = get_streamfile_size(streamFile) - 0x10 - 0x10;

    /* check extensions */
    if ( !check_extensions(streamFile, "fsb,wii") )
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x00574156) /* "\0WAV" */
        goto fail;

    /* parse FSB subfile */
    test_streamFile = setup_fsb4_wav_streamfile(streamFile, subfile_start,subfile_size);
    if (!test_streamFile) goto fail;

    vgmstream = init_vgmstream_fsb(test_streamFile);
    if (!vgmstream) goto fail;

    /* init the VGMSTREAM */
    close_streamfile(test_streamFile);
    return vgmstream;

fail:
    close_streamfile(test_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

static STREAMFILE* setup_fsb4_wav_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"fsb");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
