#include "meta.h"
#include "../coding/coding.h"


/* ************************************************************************************************************
 * FSB defines, copied from the public spec (https://www.fmod.org/questions/question/forum-4928/)
 * The format is mostly compatible for FSB1/2/3/4, but not FSB5. Headers always use LE. A FSB contains
 *  main header + sample header(s) + raw data. In multistreams N sample headers are stored (and
 *  if the BASICHEADERS flag is set, all headers but the first use HEADER_BASIC = numsamples + datasize)
 * ************************************************************************************************************ */
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
typedef struct {
    /* main header */
    uint32_t id;
    int32_t  total_subsongs;
    uint32_t sample_header_size; /* all of the sample headers including extended information */
    uint32_t data_size;
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

    meta_t meta_type;
    off_t name_offset;
    size_t name_size;
} fsb_header;

/* ********************************************************************************** */

/* FSB4 */
VGMSTREAM * init_vgmstream_fsb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t custom_data_offset;
    int loop_flag = 0;
    int target_subsong = streamFile->stream_index;
    fsb_header fsb = {0};


    /* check extensions (.bnk = Hard Corps Uprising PS3) */
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
        fsb.data_size = read_32bitLE(0x08,streamFile);
        fsb.sample_header_size = 0x40;
        fsb.version = 0;
        fsb.flags = 0;

        if (fsb.total_subsongs > 1) goto fail;

        /* sample header (first stream only, not sure if there are multi-FSB1) */
        {
            off_t s_off = fsb.base_header_size;

            fsb.name_offset = s_off;
            fsb.name_size   = 0x20;
            fsb.num_samples = read_32bitLE(s_off+0x20,streamFile);
            fsb.stream_size = read_32bitLE(s_off+0x24,streamFile);
            fsb.sample_rate = read_32bitLE(s_off+0x28,streamFile);
            /* 0x2c:?  0x2e:?  0x30:?  0x32:? */
            fsb.mode        = read_32bitLE(s_off+0x34,streamFile);
            fsb.loop_start  = read_32bitLE(s_off+0x38,streamFile);
            fsb.loop_end    = read_32bitLE(s_off+0x3c,streamFile);

            fsb.channels = (fsb.mode & FSOUND_STEREO) ? 2 : 1;
            if (fsb.loop_end > fsb.num_samples) /* this seems common... */
                fsb.num_samples = fsb.loop_end;
            
            start_offset = fsb.base_header_size + fsb.sample_header_size;
            custom_data_offset = fsb.base_header_size + fsb.sample_header_min; /* DSP coefs, seek tables, etc */
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
        fsb.sample_header_size = read_32bitLE(0x08,streamFile);
        fsb.data_size = read_32bitLE(0x0c,streamFile);
        if (fsb.base_header_size > 0x10) {
            fsb.version = read_32bitLE(0x10,streamFile);
            fsb.flags   = read_32bitLE(0x14,streamFile);
            /* FSB4: 0x18:hash  0x20:guid */
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

        if (fsb.sample_header_size < fsb.sample_header_min) goto fail;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > fsb.total_subsongs || fsb.total_subsongs < 1) goto fail;

        /* sample header (N-stream) */
        {
            int i;
            off_t s_off = fsb.base_header_size;
            off_t d_off = fsb.base_header_size + fsb.sample_header_size;

            /* find target_stream header (variable sized) */
            for (i = 0; i < fsb.total_subsongs; i++) {
                size_t stream_header_size = (uint16_t)read_16bitLE(s_off+0x00,streamFile);
                fsb.name_offset = s_off+0x02;
                fsb.name_size   = 0x20-0x02;
                fsb.num_samples = read_32bitLE(s_off+0x20,streamFile);
                fsb.stream_size = read_32bitLE(s_off+0x24,streamFile);
                fsb.loop_start  = read_32bitLE(s_off+0x28,streamFile);
                fsb.loop_end    = read_32bitLE(s_off+0x2c,streamFile);
                fsb.mode        = read_32bitLE(s_off+0x30,streamFile);
                fsb.sample_rate = read_32bitLE(s_off+0x34,streamFile);
                /* 0x38:defvol  0x3a:defpan  0x3c:defpri */
                fsb.channels = read_16bitLE(s_off+0x3e,streamFile);
                /* FSB3.1/4: 0x40:mindistance  0x44:maxdistance  0x48:varfreq/size_32bits  0x4c:varvol  0x4e:fsb.varpan */
                /* FSB3/4: 0x50:extended_data size_32bits (not always given) */

                if (i+1 == target_subsong) /* d_off found */
                    break;

                s_off += stream_header_size;
                d_off += fsb.stream_size; /* there is no offset so manually count */

                /* IMAs streams have weird end padding (maybe: FSB3=no padding, FSB4=always padding) */
                if ((fsb.mode & FSOUND_IMAADPCM) && (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED4)) {
                    if (d_off % 0x20)
                        d_off += 0x20 - (d_off % 0x20);
                }
            }
            if (i > fsb.total_subsongs) goto fail; /* not found */

            start_offset = d_off;
            custom_data_offset = s_off + fsb.sample_header_min; /* DSP coefs, seek tables, etc */
        }
    }


    /* XOR encryption for some FSB4, though the flag is only seen after decrypting */
    //VGM_ASSERT(fsb.flags & FMOD_FSB_SOURCE_ENCRYPTED, "FSB ENCRYPTED found\n");

    /* sometimes there is garbage at the end or missing bytes due to improper demuxing */
    VGM_ASSERT(fsb.base_header_size + fsb.sample_header_size + fsb.data_size != streamFile->get_size(streamFile),
               "FSB wrong head/data_size found (expected 0x%x vs 0x%x)\n",
               fsb.base_header_size + fsb.sample_header_size + fsb.data_size, streamFile->get_size(streamFile));

    /* Loops unless disabled. FMOD default seems full loops (0/num_samples-1) without flags, for repeating tracks
     * that should loop and jingles/sfx that shouldn't. We'll try to disable looping if it looks jingly enough. */
    loop_flag = !(fsb.mode & FSOUND_LOOP_OFF);
    if(!(fsb.mode & FSOUND_LOOP_NORMAL)                             /* rarely set */
            && fsb.loop_start+fsb.loop_end+1 == fsb.num_samples   /* full loop */
            && fsb.num_samples < 20*fsb.sample_rate)                  /* seconds, lame but no other way to know */
        loop_flag = 0;

    /* ping-pong looping = no looping? (forward > reverse > forward) */
    VGM_ASSERT(fsb.mode & FSOUND_LOOP_BIDI, "FSB BIDI looping found\n");


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsb.channels,loop_flag);
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


    /* parse codec */
    if (fsb.mode & FSOUND_MPEG) { /* FSB4: Shatter, Way of the Samurai 3/4 (PS3) */
#if defined(VGM_USE_MPEG)
        mpeg_custom_config cfg = {0};

        cfg.fsb_padding = (vgmstream->channels > 2 ? 16 :
            (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED4 ? 4 :
            (fsb.flags & FMOD_FSB_SOURCE_MPEG_PADDED ? 2 : 0)));

        //VGM_ASSERT(fsb.mode & FSOUND_MPEG_LAYER2, "FSB FSOUND_MPEG_LAYER2 found\n");/* not always set anyway */
        VGM_ASSERT(fsb.mode & FSOUND_IGNORETAGS, "FSB FSOUND_IGNORETAGS found\n");

        vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_FSB, &cfg);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->layout_type = layout_none;
#else
        goto fail; /* FFmpeg can't properly read FSB4 or FMOD's 0-padded MPEG data @ start_offset */
#endif
    }
    else if (fsb.mode & FSOUND_IMAADPCM) { /* FSB3: Bioshock (PC); FSB4: Blade Kitten (PC) */
        /* FSOUND_IMAADPCMSTEREO is "noninterleaved, true stereo IMA", but doesn't seem to be any different
         * (found in FSB4: Shatter, Blade Kitten (PC), Hard Corps: Uprising (PS3)) */

        vgmstream->coding_type = coding_XBOX_IMA;
        vgmstream->layout_type = layout_none;
        /* "interleaved header" IMA, only used with >2ch (ex. Blade Kitten 6ch)
         * or (seemingly) when flag is used (ex. Dead to Rights 2 (Xbox) 2ch in FSB3.1 */
        if (vgmstream->channels > 2 || (fsb.mode & FSOUND_MULTICHANNEL))
            vgmstream->coding_type = coding_FSB_IMA;
    }
    else if (fsb.mode & FSOUND_VAG) { /* FSB1: Jurassic Park Operation Genesis (PS2), FSB4: Spider Man Web of Shadows (PSP) */
        vgmstream->coding_type = coding_PSX;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x10;
    }
    else if (fsb.mode & FSOUND_XMA) { /* FSB4: Armored Core V (X360), Hard Corps (X360) */
#if defined(VGM_USE_FFMPEG)
        uint8_t buf[0x100];
        size_t bytes, block_size, block_count;

        block_size = 0x8000; /* FSB default */
        block_count = fsb.stream_size / block_size; /* not accurate but not needed (custom_data_offset+0x14 -1?) */

        bytes = ffmpeg_make_riff_xma2(buf, 0x100, fsb.num_samples, fsb.stream_size, fsb.channels, fsb.sample_rate, block_count, block_size);
        if (bytes <= 0) goto fail;

        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,fsb.stream_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
#else
        goto fail;
#endif
    }
    else if (fsb.mode & FSOUND_GCADPCM) {
        /* FSB3: ?; FSB4: de Blob (Wii), Night at the Museum, M. Night Shyamalan Avatar: The Last Airbender */
        vgmstream->coding_type = coding_NGC_DSP_subint;
        vgmstream->layout_type = layout_none;
        vgmstream->interleave_block_size = 0x2;
        dsp_read_coefs_be(vgmstream, streamFile, custom_data_offset, 0x2e);
    }
    else if (fsb.mode & FSOUND_CELT) { /* FSB4: War Thunder (PC), The Witcher 2 (PC) */
        VGM_LOG("FSB4 FSOUND_CELT found\n");
        goto fail;
    }
    else { /* PCM */
        if (fsb.mode & FSOUND_8BITS) {
            vgmstream->coding_type = (fsb.mode & FSOUND_UNSIGNED) ? coding_PCM8_U : coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x1;
        }
        else { /* Rocket Knight (PC), Another Century's Episode R (PS3), Toy Story 3 (Wii)  */
            /* sometimes FSOUND_STEREO/FSOUND_MONO is not set (ex. Dead Space iOS),
             * or only STEREO/MONO but not FSOUND_8BITS/FSOUND_16BITS is set */
            vgmstream->coding_type = (fsb.flags & FMOD_FSB_SOURCE_BIGENDIANPCM) ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
        }
    }

    /* full channel interleave, used in short streams (ex. de Blob Wii SFXs) */
    if (fsb.channels > 1 && (fsb.flags & FMOD_FSB_SOURCE_NOTINTERLEAVED)) {
        if (vgmstream->coding_type == coding_NGC_DSP_subint)
            vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = fsb.stream_size / fsb.channels;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static STREAMFILE* setup_fsb4_wav_streamfile(STREAMFILE *streamfile, off_t subfile_offset, size_t subfile_size);

/* FSB4 with "\0WAV" Header, found in Deadly Creatures (Wii).
 * Has a 0x10 BE header that holds the filesize (unsure if this is from a proper rip). */
VGMSTREAM * init_vgmstream_fsb4_wav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *test_streamFile = NULL;
    off_t subfile_start = 0x10;
    size_t subfile_size = get_streamfile_size(streamFile) - 0x10 - 0x10; //todo

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
