#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "fsb_interleave_streamfile.h"
#include "fsb_fev.h"


typedef enum { NONE, MPEG, XBOX_IMA, FSB_IMA, PSX, XMA1, XMA2, DSP, CELT, PCM8, PCM8U, PCM16LE, PCM16BE, SILENCE } fsb_codec_t;
typedef struct {
    /* main header */
    uint32_t id;
    int32_t  total_subsongs;
    uint32_t sample_headers_size;
    uint32_t sample_data_size;
    uint32_t version;   /* extended fsb version (in FSB 3/3.1/4) */
    uint32_t flags;     /* flags common to all streams (in FSB 3/3.1/4) */
    /* sample header */
    uint32_t stream_size;
    int32_t  num_samples;
    int32_t  loop_start;
    int32_t  loop_end;
    int32_t  sample_rate;
    uint16_t channels;
    uint32_t mode;      /* flags for current stream */
    /* extra */
    uint32_t base_header_size;
    uint32_t sample_header_min;
    off_t extradata_offset;
    off_t first_extradata_offset;

    meta_t meta_type;
    off_t name_offset;
    size_t name_size;

    int mpeg_padding;
    bool non_interleaved;

    bool loop_flag;

    uint32_t stream_offset;

    fsb_codec_t codec;
} fsb_header_t;

static bool parse_fsb(fsb_header_t* fsb, STREAMFILE* sf);
static void get_name(char* buf, fsb_header_t* fsb, STREAMFILE* sf_fsb);
static layered_layout_data* build_layered_fsb_celt(STREAMFILE* sf, fsb_header_t* fsb, bool is_new_lib);

/* FSB1~4 - from games using FMOD audio middleware */
VGMSTREAM* init_vgmstream_fsb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    fsb_header_t fsb = {0};


    /* checks */
    uint32_t id = read_u32be(0x00, sf);
    if (id < get_id32be("FSB1") || id > get_id32be("FSB4"))
        return NULL;

    /* .fsb: standard
     * .bnk: Hard Corps Uprising (PS3)
     * .sfx: Geon Cube (Wii)
     * .ps3: Neversoft games (PS3)
     * .xen: Neversoft games (X360/PC) */
    if (!check_extensions(sf, "fsb,bnk,sfx,ps3,xen"))
        return NULL;

    if (!parse_fsb(&fsb, sf))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsb.channels, fsb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fsb.sample_rate;
    vgmstream->num_samples = fsb.num_samples;
    vgmstream->loop_start_sample = fsb.loop_start;
    vgmstream->loop_end_sample = fsb.loop_end;
    vgmstream->num_streams = fsb.total_subsongs;
    vgmstream->stream_size = fsb.stream_size;
    vgmstream->meta_type = fsb.meta_type;
    get_name(vgmstream->stream_name, &fsb, sf);

    switch(fsb.codec) {

#ifdef VGM_USE_MPEG
        case MPEG: {    /* FSB3: Spider-man 3 (PC/PS3), Rise of the Argonauts (PC), FSB4: Shatter (PS3), Way of the Samurai 3/4 (PS3) */
            mpeg_custom_config cfg = {0};

            cfg.fsb_padding = fsb.mpeg_padding; // frames are sometimes padded for alignment
            cfg.data_size = fsb.stream_offset + fsb.stream_size;

            vgmstream->codec_data = init_mpeg_custom(sf, fsb.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
        case XBOX_IMA:  /* FSB3: Bioshock (PC), FSB4: Blade Kitten (PC) */
        case FSB_IMA:   /* FSB3: Dead to Rights 2 (Xbox)-2ch, FSB4: Blade Kitten (PC)-6ch */
            vgmstream->coding_type = fsb.codec == FSB_IMA ? coding_FSB_IMA : coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case PSX:       /* FSB1: Jurassic Park Operation Genesis (PS2), FSB4: Spider Man Web of Shadows (PSP) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (fsb.non_interleaved) {
                vgmstream->interleave_block_size = fsb.stream_size / fsb.channels;
            }
            else {
                vgmstream->interleave_block_size = 0x10;
            }
            break;

#ifdef VGM_USE_FFMPEG
        case XMA1:      /* FSB3: The Bourne Conspiracy 2008 (X360), Forza Motorsport 23 (X360) */
        case XMA2: {    /* FSB4: Armored Core V (X360), Hard Corps (X360) */
            if (fsb.codec == XMA1) {
                /* 3.x, though no actual output changes [Guitar Hero III (X360), The Bourne Conspiracy (X360)] */
                vgmstream->codec_data = init_ffmpeg_xma1_raw(sf, fsb.stream_offset, fsb.stream_size, fsb.channels, fsb.sample_rate, 0);
            }
            else {
                int block_size = 0x8000; /* FSB default */
                vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, fsb.stream_offset, fsb.stream_size, fsb.num_samples, fsb.channels, fsb.sample_rate, block_size, 0);
            }
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, fsb.stream_offset,fsb.stream_size, 0, 0,0); /* samples look ok */
            break;
        }
#endif

        case DSP:       /* FSB3: Metroid Prime 3 (GC), FSB4: de Blob (Wii) */
            if (fsb.non_interleaved) { /* [de Blob (Wii) sfx)] */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = fsb.stream_size / fsb.channels;
            }
            else {
                vgmstream->coding_type = coding_NGC_DSP_subint;
                vgmstream->layout_type = layout_none;
                vgmstream->interleave_block_size = 0x2;
            }
            dsp_read_coefs_be(vgmstream, sf, fsb.extradata_offset, 0x2e);
            break;

#ifdef VGM_USE_CELT
        case CELT: {    /* FSB4: War Thunder (PC), The Witcher 2 (PC), Vessel (PC) */
            bool is_new_lib;

            /* get libcelt version (set in the first subsong only, but try all extradata just in case) */
            if (fsb.first_extradata_offset || fsb.extradata_offset) {
                uint32_t lib = fsb.first_extradata_offset ?
                        read_u32le(fsb.first_extradata_offset, sf) :
                        read_u32le(fsb.extradata_offset, sf);
                switch(lib) {
                    case 0x80000009: is_new_lib = false; break; /* War Thunder (PC) */
                    case 0x80000010: is_new_lib = true; break; /* Vessel (PC) */
                    default: VGM_LOG("FSB: unknown CELT lib 0x%x\n", lib); goto fail;
                }
            }
            else {
                /* split FSBs? try to guess from observed bitstreams */
                uint16_t frame = read_u16be(fsb.stream_offset+0x04+0x04,sf);
                if ((frame & 0xF000) == 0x6000 || frame == 0xFFFE) {
                    is_new_lib = true;
                }
                else {
                    is_new_lib = false;
                }
            }

            if (fsb.channels > 2) { /* multistreams */
                vgmstream->layout_data = build_layered_fsb_celt(sf, &fsb, is_new_lib);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_layered;
            }
            else {
                vgmstream->codec_data = is_new_lib ? 
                    init_celt_fsb_v2(vgmstream->channels) :
                    init_celt_fsb_v1(vgmstream->channels);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_none;
            }

            break;
        }
#endif

        case PCM8:      /* no known games */
        case PCM8U:     /* FSB4: Crash Time 4: The Syndicate (X360), Zoombinis (PC) */
            vgmstream->coding_type = (fsb.codec == PCM8U) ? coding_PCM8_U : coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x1;
            break;

        case PCM16LE:   /* FSB2: Hot Wheels World Race (PC)-bigfile-sfx, FSB3: Bee Movie (Wii), FSB4: Rocket Knight (PC), Another Century's Episode R (PS3), Toy Story 3 (Wii) */
        case PCM16BE:   /* FSB4: SpongeBob's Truth or Square (X360), Crash Time 4: The Syndicate (X360) */
            vgmstream->coding_type = (fsb.codec == PCM16BE) ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
            break;

        case SILENCE:   /* special case for broken MPEG */
            vgmstream->coding_type = coding_SILENCE;
            vgmstream->layout_type = layout_none;
            break;

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, fsb.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_CELT
static layered_layout_data* build_layered_fsb_celt(STREAMFILE* sf, fsb_header_t* fsb, bool is_new_lib) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    int layers = (fsb->channels+1) / 2;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (1/2ch CELT streams: 2ch+2ch..+1ch or 2ch+2ch..+2ch) */
    for (int i = 0; i < layers; i++) {
        int layer_channels = (i+1 == layers && fsb->channels % 2 == 1)
                ? 1 : 2; /* last layer can be 1/2ch */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, fsb->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = fsb->sample_rate;
        data->layers[i]->num_samples = fsb->num_samples;
        data->layers[i]->loop_start_sample = fsb->loop_start;
        data->layers[i]->loop_end_sample = fsb->loop_end;

#ifdef VGM_USE_CELT
        data->layers[i]->codec_data = is_new_lib ? 
            init_celt_fsb_v2(layer_channels) : 
            init_celt_fsb_v1(layer_channels);
        if (!data->layers[i]->codec_data) goto fail;
        data->layers[i]->coding_type = coding_CELT_FSB;
        data->layers[i]->layout_type = layout_none;
#else
        goto fail;
#endif

        temp_sf = setup_fsb_interleave_streamfile(sf, fsb->stream_offset, fsb->stream_size, layers, i, FSB_INT_CELT);
        if (!temp_sf) goto fail;

        if (!vgmstream_open_stream(data->layers[i], temp_sf, 0x00))
            goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    return data;

fail:
    close_streamfile(temp_sf);
    free_layout_layered(data);
    return NULL;
}
#endif


/* FSB defines, copied from the public spec (https://www.fmod.org/questions/question/forum-4928/)
 * for reference. The format is mostly compatible between FSB1/2/3/4, but not FSB5. */

/* These flags are used for FMOD_FSB_HEADER::mode */
#define FMOD_FSB_SOURCE_FORMAT          0x00000001  /* all samples stored in their original compressed format */
#define FMOD_FSB_SOURCE_BASICHEADERS    0x00000002  /* samples should use the basic header structure */
#define FMOD_FSB_SOURCE_ENCRYPTED       0x00000004  /* all sample data is encrypted */
#define FMOD_FSB_SOURCE_BIGENDIANPCM    0x00000008  /* pcm samples have been written out in big-endian format */
#define FMOD_FSB_SOURCE_NOTINTERLEAVED  0x00000010  /* Sample data is not interleaved. */
#define FMOD_FSB_SOURCE_MPEG_PADDED     0x00000020  /* Mpeg frames are now rounded up to the nearest 2 bytes for normal sounds, or 16 bytes for multichannel. */
#define FMOD_FSB_SOURCE_MPEG_PADDED4    0x00000040  /* Mpeg frames are now rounded up to the nearest 4 bytes for normal sounds, or 16 bytes for multichannel. */

/* These flags are used for FMOD_FSB_HEADER::version */
#define FMOD_FSB_VERSION_3_0            0x00030000  /* FSB version 3.0 */
#define FMOD_FSB_VERSION_3_1            0x00030001  /* FSB version 3.1 */
#define FMOD_FSB_VERSION_4_0            0x00040000  /* FSB version 4.0 */

/* FMOD 3 defines.  These flags are used for FMOD_FSB_SAMPLE_HEADER::mode */
#define FSOUND_LOOP_OFF                 0x00000001  /* For non looping samples. */
#define FSOUND_LOOP_NORMAL              0x00000002  /* For forward looping samples. */
#define FSOUND_LOOP_BIDI                0x00000004  /* For bidirectional looping samples.  (no effect if in hardware). */
#define FSOUND_8BITS                    0x00000008  /* For 8 bit samples. */
#define FSOUND_16BITS                   0x00000010  /* For 16 bit samples. */
#define FSOUND_MONO                     0x00000020  /* For mono samples. */
#define FSOUND_STEREO                   0x00000040  /* For stereo samples. */
#define FSOUND_UNSIGNED                 0x00000080  /* For user created source data containing unsigned samples. */
#define FSOUND_SIGNED                   0x00000100  /* For user created source data containing signed data. */
#define FSOUND_MPEG                     0x00000200  /* For MPEG layer 2/3 data. */
#define FSOUND_CHANNELMODE_ALLMONO      0x00000400  /* Sample is a collection of mono channels. */
#define FSOUND_CHANNELMODE_ALLSTEREO    0x00000800  /* Sample is a collection of stereo channel pairs */
#define FSOUND_HW3D                     0x00001000  /* Attempts to make samples use 3d hardware acceleration. (if the card supports it) */
#define FSOUND_2D                       0x00002000  /* Tells software (not hardware) based sample not to be included in 3d processing. */
#define FSOUND_SYNCPOINTS_NONAMES       0x00004000  /* Specifies that syncpoints are present with no names */
#define FSOUND_DUPLICATE                0x00008000  /* This subsound is a duplicate of the previous one i.e. it uses the same sample data but w/different mode bits */
#define FSOUND_CHANNELMODE_PROTOOLS     0x00010000  /* Sample is 6ch and uses L C R LS RS LFE standard. */
#define FSOUND_MPEGACCURATE             0x00020000  /* For FSOUND_Stream_Open - for accurate FSOUND_Stream_GetLengthMs/FSOUND_Stream_SetTime.  WARNING, see FSOUND_Stream_Open for inital opening time performance issues. */
#define FSOUND_HW2D                     0x00080000  /* 2D hardware sounds.  allows hardware specific effects */
#define FSOUND_3D                       0x00100000  /* 3D software sounds */
#define FSOUND_32BITS                   0x00200000  /* For 32 bit (float) samples. */
#define FSOUND_IMAADPCM                 0x00400000  /* Contents are stored compressed as IMA ADPCM */
#define FSOUND_VAG                      0x00800000  /* For PS2 only - Contents are compressed as Sony VAG format */
#define FSOUND_XMA                      0x01000000  /* For Xbox360 only - Contents are compressed as XMA format */
#define FSOUND_GCADPCM                  0x02000000  /* For Gamecube only - Contents are compressed as Gamecube DSP-ADPCM format */
#define FSOUND_MULTICHANNEL             0x04000000  /* For PS2 and Gamecube only - Contents are interleaved into a multi-channel (more than stereo) format */
#define FSOUND_OGG                      0x08000000  /* For vorbis encoded ogg data */
#define FSOUND_CELT                     0x08000000  /* For vorbis encoded ogg data */
#define FSOUND_MPEG_LAYER3              0x10000000  /* Data is in MP3 format. */
#define FSOUND_MPEG_LAYER2              0x00040000  /* Data is in MP2 format. */
#define FSOUND_LOADMEMORYIOP            0x20000000  /* For PS2 only - &quot;name&quot; will be interpreted as a pointer to data for streaming and samples.  The address provided will be an IOP address */
#define FSOUND_IMAADPCMSTEREO           0x20000000  /* Signify IMA ADPCM is actually stereo not two interleaved mono */
#define FSOUND_IGNORETAGS               0x40000000  /* Skips id3v2 etc tag checks when opening a stream, to reduce seek/read overhead when opening files (helps with CD performance) */
#define FSOUND_SYNCPOINTS               0x80000000  /* Specifies that syncpoints are present */

/* These flags are used for FMOD_FSB_SAMPLE_HEADER::mode */
#define FSOUND_CHANNELMODE_MASK         (FSOUND_CHANNELMODE_ALLMONO | FSOUND_CHANNELMODE_ALLSTEREO | FSOUND_CHANNELMODE_PROTOOLS)
#define FSOUND_CHANNELMODE_DEFAULT      0x00000000  /* Determine channel assignment automatically from channel count. */
#define FSOUND_CHANNELMODE_RESERVED     0x00000C00
#define FSOUND_NORMAL                   (FSOUND_16BITS | FSOUND_SIGNED | FSOUND_MONO)
#define FSB_SAMPLE_DATA_ALIGN           32


/* FSB loop info/flags are kind of wonky, try to make sense of them */
static void fix_loops(fsb_header_t* fsb) {
    /* ping-pong looping = no looping? (forward > reverse > forward) [Biker Mice from Mars (PS2)] */
    VGM_ASSERT(fsb->mode & FSOUND_LOOP_BIDI, "FSB BIDI looping found\n");
    VGM_ASSERT(fsb->mode & FSOUND_LOOP_OFF, "FSB LOOP OFF found\n"); /* sometimes used */
    VGM_ASSERT(fsb->mode & FSOUND_LOOP_NORMAL, "FSB LOOP NORMAL found\n"); /* very rarely set */

    /* FMOD tool's default behaviour is creating files with full loops and no flags unless disabled
     * manually via flag (can be overriden during program too), for all FSB versions. This makes jingles/sfx/voices
     * loop when they shouldn't, but most music does full loops seamlessly, so we only want to disable
     * if it looks jingly enough. Incidentally, their tools can only make files with full loops. */
    bool enable_loop, full_loop, is_small;

    /* seems to mean forced loop */
    enable_loop = (fsb->mode & FSOUND_LOOP_NORMAL);

    /* for MPEG and CELT sometimes full loops are given with around/exact 1 frame less than num_samples,
     * probably to account for encoder/decoder delay (ex. The Witcher 2, Hard Reset, Timeshift) */
    if (fsb->codec == CELT)
        full_loop = fsb->loop_start - 512 <= 0 && fsb->loop_end >= fsb->num_samples - 512; /* aproximate */
    else if (fsb->codec == MPEG)
        full_loop = fsb->loop_start - 1152 <= 0 && fsb->loop_end >= fsb->num_samples - 1152; /* WWF Legends of Wrestlemania uses 2 frames? */
    else
        full_loop = fsb->loop_start == 0 && fsb->loop_end == fsb->num_samples;

    /* in seconds (lame but no better way) */
    is_small = fsb->num_samples < 20 * fsb->sample_rate;

    //;VGM_LOG("FSB: loop start=%i, loop end=%i, samples=%i, mode=%x\n", fsb->loop_start, fsb->loop_end, fsb->num_samples, fsb->mode);
    //;VGM_LOG("FSB: enable=%i, full=%i, small=%i\n",enable_loop,full_loop,is_small );

    fsb->loop_flag = !(fsb->mode & FSOUND_LOOP_OFF); /* disabled manually */
    if (fsb->loop_flag && !enable_loop && full_loop && is_small) {
        VGM_LOG("FSB: disabled unwanted loop\n");
        fsb->loop_flag = false;
    }
}

/* covert codec info incomprehensibly defined as bitflags into single codec */
static void load_codec(fsb_header_t* fsb) {
    // for rare cases were codec is preloaded while reading the header
    if (fsb->codec)
        return;

    if      (fsb->mode & FSOUND_MPEG)        fsb->codec = MPEG;
    else if (fsb->mode & FSOUND_IMAADPCM)    fsb->codec = XBOX_IMA;
    else if (fsb->mode & FSOUND_VAG)         fsb->codec = PSX;
    else if (fsb->mode & FSOUND_XMA)         fsb->codec = (fsb->version != FMOD_FSB_VERSION_4_0) ? XMA1 : XMA2;
    else if (fsb->mode & FSOUND_GCADPCM)     fsb->codec = DSP;
    else if (fsb->mode & FSOUND_CELT)        fsb->codec = CELT;
    else if (fsb->mode & FSOUND_8BITS)       fsb->codec = (fsb->mode & FSOUND_UNSIGNED) ? PCM8U : PCM8;
    else                                     fsb->codec = (fsb->flags & FMOD_FSB_SOURCE_BIGENDIANPCM) ? PCM16BE : PCM16LE;

    if (fsb->codec == MPEG) {
        //VGM_ASSERT(fsb->mode & FSOUND_MPEG_LAYER2, "FSB FSOUND_MPEG_LAYER2 found\n");/* not always set anyway */
        VGM_ASSERT(fsb->mode & FSOUND_IGNORETAGS, "FSB FSOUND_IGNORETAGS found\n"); /* mpeg only? not seen */

        fsb->mpeg_padding = (fsb->channels > 2 ? 0x10 :
            (fsb->flags & FMOD_FSB_SOURCE_MPEG_PADDED4 ? 0x04 :
            (fsb->flags & FMOD_FSB_SOURCE_MPEG_PADDED ? 0x02 : 0x000)));
    }

    if (fsb->codec == XBOX_IMA) {
        /* "interleaved header" IMA, only used with >2ch [Blade Kitten (PC)-6ch]
         * or (seemingly) when flag is used [Dead to Rights 2 (Xbox)- 2ch in FSB3.1] */
        if (fsb->channels > 2 || (fsb->mode & FSOUND_MULTICHANNEL))
            fsb->codec = FSB_IMA;

        /* FSOUND_IMAADPCMSTEREO is "noninterleaved, true stereo IMA", but doesn't seem to be any different
            * (found in FSB4: Shatter, Blade Kitten (PC), Hard Corps: Uprising (PS3)) */
    }


    /* officially only for PSX/DSP */
    if (fsb->codec == PSX || fsb->codec == DSP) {
        fsb->non_interleaved = fsb->flags & FMOD_FSB_SOURCE_NOTINTERLEAVED;
    }


    /* PCM16: sometimes FSOUND_MONO/FSOUND_STEREO is not set [Dead Space (iOS)]
     * or only STEREO/MONO but not FSOUND_8BITS/FSOUND_16BITS is set */
}


static bool parse_fsb(fsb_header_t* fsb, STREAMFILE* sf) {
    int target_subsong = sf->stream_index;

    fsb->id = read_u32be(0x00,sf);
    if (fsb->id == get_id32be("FSB1")) {
        fsb->meta_type = meta_FSB1;
        fsb->base_header_size = 0x10;
        fsb->sample_header_min = 0x40;

        /* main header */
        fsb->total_subsongs     = read_s32le(0x04,sf);
        fsb->sample_data_size   = read_u32le(0x08,sf);
        fsb->sample_headers_size = 0x40;
        fsb->version = 0;
        fsb->flags = 0;

        if (fsb->total_subsongs > 1)
            return false;

        /* sample header (first stream only, not sure if there are multi-FSB1) */
        {
            off_t header_offset = fsb->base_header_size;

            fsb->name_offset = header_offset;
            fsb->name_size   = 0x20;
            fsb->num_samples = read_s32le(header_offset+0x20,sf);
            fsb->stream_size = read_u32le(header_offset+0x24,sf);
            fsb->sample_rate = read_s32le(header_offset+0x28,sf);
            // 0x2c: ?
            // 0x2e: ?
            // 0x30: ?
            // 0x32: ?
            fsb->mode        = read_u32le(header_offset+0x34,sf);
            fsb->loop_start  = read_s32le(header_offset+0x38,sf);
            fsb->loop_end    = read_s32le(header_offset+0x3c,sf);

            fsb->channels = (fsb->mode & FSOUND_STEREO) ? 2 : 1;
            if (fsb->loop_end > fsb->num_samples) /* this seems common... */
                fsb->num_samples = fsb->loop_end;

            /* DSP coefs, seek tables, etc */
            fsb->extradata_offset = header_offset+fsb->sample_header_min;

            fsb->stream_offset = fsb->base_header_size + fsb->sample_headers_size;
        }
    }
    else {
        if (fsb->id == get_id32be("FSB2")) {
            fsb->meta_type = meta_FSB2;
            fsb->base_header_size  = 0x10;
            fsb->sample_header_min = 0x40;
        }
        else if (fsb->id == get_id32be("FSB3")) {
            fsb->meta_type = meta_FSB3;
            fsb->base_header_size  = 0x18;
            fsb->sample_header_min = 0x40;
        }
        else if (fsb->id == get_id32be("FSB4")) {
            fsb->meta_type = meta_FSB4;
            fsb->base_header_size  = 0x30;
            fsb->sample_header_min = 0x50;
        }
        else {
            goto fail;
        }

        /* main header */
        fsb->total_subsongs         = read_s32le(0x04,sf);
        fsb->sample_headers_size    = read_u32le(0x08,sf);
        fsb->sample_data_size       = read_u32le(0x0c,sf);
        if (fsb->base_header_size > 0x10) {
            fsb->version    = read_u32le(0x10,sf);
            fsb->flags      = read_u32le(0x14,sf);
            /* FSB4 only: */
            // 0x18(8): hash
            // 0x20(10): guid
        } else {
            fsb->version = 0;
            fsb->flags   = 0;
        }

        if (fsb->version == FMOD_FSB_VERSION_3_1) {
            fsb->sample_header_min = 0x50;
        }
        else if (fsb->version != 0 /* FSB2 */
                && fsb->version != FMOD_FSB_VERSION_3_0
                && fsb->version != FMOD_FSB_VERSION_4_0) {
            goto fail;
        }

        if (fsb->sample_headers_size < fsb->sample_header_min) goto fail;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > fsb->total_subsongs || fsb->total_subsongs < 1) goto fail;

        /* sample header (N-stream) */
        {
            int i;
            uint32_t header_offset = fsb->base_header_size;
            uint32_t data_offset = fsb->base_header_size + fsb->sample_headers_size;

            /* find target_stream header (variable sized) */
            for (i = 0; i < fsb->total_subsongs; i++) {
                uint32_t stream_header_size;
                bool null_name = false;

                if ((fsb->flags & FMOD_FSB_SOURCE_BASICHEADERS) && i > 0) {
                    /* miniheader, all subsongs reuse first header (rare) [Biker Mice from Mars (PS2)] */
                    stream_header_size  = 0x08;
                    fsb->num_samples    = read_s32le(header_offset+0x00,sf);
                    fsb->stream_size    = read_u32le(header_offset+0x04,sf);
                    fsb->loop_start     = 0;
                    fsb->loop_end       = 0;

                    /* XMA basic headers have extra data [Forza Motorsport 3 (X360)] */
                    if (fsb->mode & FSOUND_XMA) {
                        // 0x08: flags? (0x00=none?, 0x20=standard)
                        // 0x0c: sample related? (may be 0 with no seek table)
                        // 0x10: low number (may be 0 with no seek table)
                        uint32_t seek_size = read_u32le(header_offset+0x14, sf); /* may be 0 */

                        stream_header_size += 0x10 + seek_size;

                        /* seek table format: */
                        // 0x00: always 0x01?
                        // 0x04: seek entries? when 'flags' == 0x00 may be bigger than actual entries (flag means "no table" but space is still reserved?)
                        // per entry:
                        //   0x00: block offset
                    }
                }
                else {
                    /* subsong header for normal files */
                    stream_header_size  = read_u16le(header_offset+0x00,sf);
                    fsb->name_offset    = header_offset + 0x02;
                    fsb->name_size      = 0x20 - 0x02;
                    fsb->num_samples    = read_s32le(header_offset+0x20,sf);
                    fsb->stream_size    = read_u32le(header_offset+0x24,sf);
                    fsb->loop_start     = read_s32le(header_offset+0x28,sf);
                    fsb->loop_end       = read_s32le(header_offset+0x2c,sf);
                    fsb->mode           = read_u32le(header_offset+0x30,sf);
                    fsb->sample_rate    = read_s32le(header_offset+0x34,sf);
                    // 0x38: defvol
                    // 0x3a: defpan
                    // 0x3c: defpri
                    fsb->channels       = read_u16le(header_offset+0x3e,sf);
                    /* FSB3.1/4 only: */
                    // 0x40: mindistance
                    // 0x44: maxdistance
                    // 0x48: varfreq/size_32bits
                    // 0x4c: varvol
                    // 0x4e: fsb->varpan

                    /* DSP coefs, seek tables, etc */
                    if (stream_header_size > fsb->sample_header_min) {
                        fsb->extradata_offset = header_offset + fsb->sample_header_min;
                        if (fsb->first_extradata_offset == 0)
                            fsb->first_extradata_offset = fsb->extradata_offset;
                    }

                    // Inversion (PC)-ru has some null names with garbage offsets from prev streams (not in X360/PS3)
                    // seen in MPEG and CPM16
                    if (fsb->version == FMOD_FSB_VERSION_4_0 && 
                            ((fsb->mode & FSOUND_MPEG) || (fsb->mode & FSOUND_16BITS))) {
                        null_name = read_u32le(fsb->name_offset, sf) == 0x00;
                    }
                }

                // target found
                if (i + 1 == target_subsong) {
                    if (null_name) {
                        fsb->codec = SILENCE;
                        fsb->num_samples = fsb->sample_rate;
                    }
                    break;
                }

                // must calculate final offsets
                header_offset += stream_header_size;
                if (!null_name)
                    data_offset += fsb->stream_size;

                // some subsongs offsets need padding (most FSOUND_IMAADPCM, few MPEG too [Hard Reset (PC) subsong 5])
                // other codecs may set PADDED4 (ex. XMA) but don't seem to need it and work fine
                if (fsb->flags & FMOD_FSB_SOURCE_MPEG_PADDED4) {
                    if (data_offset % 0x20)
                        data_offset += 0x20 - (data_offset % 0x20);
                }
            }

            // target not found
            if (i > fsb->total_subsongs)
                goto fail;

            fsb->stream_offset = data_offset;
        }
    }


    load_codec(fsb);

    /* correct compared to FMOD's tools */
    if (fsb->loop_end)
        fsb->loop_end += 1;

    fix_loops(fsb);

    /* sometimes there is garbage at the end or missing bytes due to improper ripping (maybe should reject them...) */
    vgm_asserti(fsb->base_header_size + fsb->sample_headers_size + fsb->sample_data_size != get_streamfile_size(sf),
               "FSB wrong head/data_size found (expected 0x%x vs 0x%x)\n",
               fsb->base_header_size + fsb->sample_headers_size + fsb->sample_data_size, (uint32_t)get_streamfile_size(sf));

    /* XOR encryption for some FSB4, though the flag is only seen after decrypting */
    //;VGM_ASSERT(fsb->flags & FMOD_FSB_SOURCE_ENCRYPTED, "FSB ENCRYPTED found\n");

#ifdef VGM_USE_MPEG
    // rare FSB3 have odd cases [Rise of the Argonauts (PC)]
    if (fsb->codec == MPEG && fsb->version == FMOD_FSB_VERSION_3_1) {
        uint32_t mpeg_id = read_u32be(fsb->stream_offset, sf);

        if ((mpeg_id & 0xFFFFFF00) == get_id32be("ID3\0")) {
            // starts with ID3, probably legal but otherwise not seen (stripped?): Lykas_Atalanta_Join_DLG.fsb, Support_Of_The_Gods*.fsb
            uint32_t tag_size = mpeg_get_tag_size(sf, fsb->stream_offset, mpeg_id); // always 0x1000, has 'PeakLevel' info
            fsb->stream_offset += tag_size;
            fsb->stream_size -= tag_size;
        }

        // completely empty MPEG, probably works by chance with OG decoder ignoring bad data: DLG_Lycomedes_Statue_*.fsb
        if (mpeg_id == 0) {
            fsb->codec = SILENCE;
        }

        // rarely sets more samples than data, must clamp reads to avoid spilling into next subsong: Player_Death_DLG.fsb, Lykas_Atalanta_Join_DLG.fsb
        // probably a bug as samples don't seem to match MPEG's 'Info' headers and can be both bigger and smaller than loop_end
    }
#endif

    return true;
fail:
    return false;
}

static void get_name(char* buf, fsb_header_t* fsb, STREAMFILE* sf_fsb) {
    STREAMFILE* sf_fev = NULL;
    fev_header_t fev = {0};

    sf_fev = open_fev_filename_pair(sf_fsb);
    if (sf_fev) {
        char filename[STREAM_NAME_SIZE];
        get_streamfile_basename(sf_fsb, filename, STREAM_NAME_SIZE);

        sf_fev->stream_index = sf_fsb->stream_index;
        if (!parse_fev(&fev, sf_fev, filename))
            vgm_logi("FSB: Failed to parse FEV1 data");
    }

    /* prioritise FEV stream names, usually the same as the FSB name just not truncated */
    /* benefits games where base names are all identical [Split/Second (PS3/X360/PC)] */
    if (fev.name_offset && fev.name_size)
        read_string(buf, fev.name_size, fev.name_offset, sf_fev);
    else if (fsb->name_offset)
        read_string(buf, fsb->name_size + 1, fsb->name_offset, sf_fsb);

    close_streamfile(sf_fev);
}
