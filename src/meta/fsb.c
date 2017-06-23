#include "meta.h"
#include "../coding/coding.h"

#define FAKE_RIFF_BUFFER_SIZE           100

static VGMSTREAM * init_vgmstream_fsb_offset(STREAMFILE *streamFile, off_t offset);

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
    int32_t  numsamples;     /* number of samples(streams) in the file */
    uint32_t shdrsize;       /* size in bytes of all of the sample headers including extended information */
    uint32_t datasize;       /* size in bytes of compressed sample data */
    /* main header: FSB 3/3.1/4 */
    uint32_t version;        /* extended fsb version */
    uint32_t flags;          /* flags that apply to all samples(streams) in the fsb */
    /* sample header */
    uint32_t lengthsamples;
    uint32_t lengthcompressedbytes;
    uint32_t loopstart;
    uint32_t loopend;
    uint32_t mode;
    int32_t  deffreq;
    uint16_t numchannels;
    /* extra */
    uint32_t hdrsize;
    uint32_t shdrsize_min;
    meta_t meta_type;
} FSB_HEADER;

/* ********************************************************************************** */

/* FSB4 */
VGMSTREAM * init_vgmstream_fsb(STREAMFILE *streamFile) {
    return init_vgmstream_fsb_offset(streamFile, 0x0);
}

/* FSB4 with "\0WAV" Header, found in Deadly Creatures (Wii)
 * 16 byte header which holds the filesize
 * (unsure if this is from a proper rip) */
VGMSTREAM * init_vgmstream_fsb4_wav(STREAMFILE *streamFile) {
    if (read_32bitBE(0x00,streamFile) != 0x00574156) /* "\0WAV" */
        return NULL;
    return init_vgmstream_fsb_offset(streamFile, 0x10);
}

VGMSTREAM * init_vgmstream_fsb_offset(STREAMFILE *streamFile, off_t offset) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t custom_data_offset;
    int loop_flag = 0;
    int target_stream = 0;

    FSB_HEADER fsbh;

    /* check extensions */
    if ( !check_extensions(streamFile, "fsb,wii") )
        goto fail;

    /* check header */
    fsbh.id = read_32bitBE(offset+0x00,streamFile);
    if (fsbh.id == 0x46534231) { /* "FSB1" (somewhat different from other fsbs) */
        fsbh.meta_type = meta_FSB1;
        fsbh.hdrsize = 0x10;
        fsbh.shdrsize_min = 0x40;

        /* main header */
        fsbh.numsamples = read_32bitLE(offset+0x04,streamFile);
        fsbh.datasize   = read_32bitLE(offset+0x08,streamFile);
        fsbh.shdrsize   = 0x40;
        fsbh.version    = 0;
        fsbh.flags      = 0;

        if (fsbh.numsamples > 1) goto fail;

        /* sample header (first stream only, not sure if there are multi-FSB1) */
        {
            off_t s_off = offset+fsbh.hdrsize;

            /* 0x00:name(len=0x20) */
            fsbh.lengthsamples = read_32bitLE(s_off+0x20,streamFile);
            fsbh.lengthcompressedbytes = read_32bitLE(s_off+0x24,streamFile);
            fsbh.deffreq   = read_32bitLE(s_off+0x28,streamFile);
            /* 0x2c:?  0x2e:?  0x30:?  0x32:? */
            fsbh.mode     = read_32bitLE(s_off+0x34,streamFile);
            fsbh.loopstart = read_32bitLE(s_off+0x38,streamFile);
            fsbh.loopend   = read_32bitLE(s_off+0x3c,streamFile);

            fsbh.numchannels = (fsbh.mode & FSOUND_STEREO) ? 2 : 1;
            if (fsbh.loopend > fsbh.lengthsamples) /* this seems common... */
                fsbh.lengthsamples = fsbh.loopend;
            
            start_offset = offset + fsbh.hdrsize + fsbh.shdrsize;
            custom_data_offset = offset + fsbh.hdrsize + fsbh.shdrsize_min; /* DSP coefs, seek tables, etc */
        }
    }
    else { /* other FSBs (common/extended format) */
        if (fsbh.id == 0x46534232) { /* "FSB2" */
            fsbh.meta_type = meta_FSB2;
            fsbh.hdrsize = 0x10;
            fsbh.shdrsize_min = 0x40; /* guessed */
        } else if (fsbh.id == 0x46534233) { /* "FSB3" */
            fsbh.meta_type = meta_FSB3;
            fsbh.hdrsize = 0x18;
            fsbh.shdrsize_min = 0x40;
        } else if (fsbh.id == 0x46534234) { /* "FSB4" */
            fsbh.meta_type = meta_FSB4;
            fsbh.hdrsize = 0x30;
            fsbh.shdrsize_min = 0x50;
        } else {
            goto fail;
        }

        /* main header */
        fsbh.numsamples = read_32bitLE(offset+0x04,streamFile);
        fsbh.shdrsize   = read_32bitLE(offset+0x08,streamFile);
        fsbh.datasize   = read_32bitLE(offset+0x0c,streamFile);
        if (fsbh.hdrsize > 0x10) {
            fsbh.version = read_32bitLE(offset+0x10,streamFile);
            fsbh.flags   = read_32bitLE(offset+0x14,streamFile);
            /* FSB4: 0x18:hash  0x20:guid */
        } else {
            fsbh.version = 0;
            fsbh.flags   = 0;
        }

        if (fsbh.version == FMOD_FSB_VERSION_3_1) {
            fsbh.shdrsize_min = 0x50;
        } else if (fsbh.version != 0 /* FSB2 */
                && fsbh.version != FMOD_FSB_VERSION_3_0
                && fsbh.version != FMOD_FSB_VERSION_4_0) {
            goto fail;
        }

        if (fsbh.shdrsize < fsbh.shdrsize_min) goto fail;
        if (target_stream == 0) target_stream = 1;
        if (target_stream < 0 || target_stream > fsbh.numsamples || fsbh.numsamples < 1) goto fail;

        /* sample header (N-stream) */
        {
            int i;
            off_t s_off = offset + fsbh.hdrsize;
            off_t d_off = offset + fsbh.hdrsize + fsbh.shdrsize;

            /* find target_stream data offset, reading each header */
            for(i=1; i <= fsbh.numsamples; i++) {
                /* 0x00:size  0x02:name(len=size) */
                fsbh.lengthsamples = read_32bitLE(s_off+0x20,streamFile);
                fsbh.lengthcompressedbytes = read_32bitLE(s_off+0x24,streamFile);
                fsbh.loopstart = read_32bitLE(s_off+0x28,streamFile);
                fsbh.loopend   = read_32bitLE(s_off+0x2c,streamFile);
                fsbh.mode      = read_32bitLE(s_off+0x30,streamFile);
                fsbh.deffreq   = read_32bitLE(s_off+0x34,streamFile);
                /* 0x38:defvol  0x3a:defpan  0x3c:defpri */
                fsbh.numchannels = read_16bitLE(s_off+0x3e,streamFile);
                /* FSB3.1/4: 0x40:mindistance  0x44:maxdistance  0x48:varfreq/size_32bits  0x4c:varvol  0x4e:fsbh.varpan */

                if (target_stream == i) /* d_off found */
                    break;

                s_off += fsbh.shdrsize_min; /* default size */
                if (fsbh.version == FMOD_FSB_VERSION_4_0) {
                    uint32_t extended_data = read_32bitLE(s_off+0x48,streamFile); /* +0x50:extended_data of size_32bits */
                    if (extended_data > fsbh.shdrsize_min)
                        s_off += extended_data;
                }

               
                d_off += fsbh.lengthcompressedbytes; /* there is no offset so manually count */
                //d_off += d_off % 0x30; /*todo some formats need padding, not sure when/how */
            }
            if (i > fsbh.numsamples) goto fail; /* not found */

            start_offset = d_off;
            custom_data_offset = s_off + fsbh.shdrsize_min; /* DSP coefs, seek tables, etc */
        }
    }

#if 0
    /* XOR encryption for some FSB4, though the flag is only seen after decrypting */
    if (fsbh.flags & FMOD_FSB_SOURCE_ENCRYPTED) {
        VGM_LOG("FSB ENCRYPTED found\n");
        goto fail;
    }

    /* sometimes there is garbage at the end or missing bytes due to improper demuxing */
    if (fsbh.hdrsize + fsbh.shdrsize + fsbh.datasize != streamFile->get_size(streamFile) - offset) {
        VGM_LOG("FSB wrong head/datasize found\n");
        goto fail;
    }
#endif

    /* Loops by default unless disabled (sometimes may add FSOUND_LOOP_NORMAL). Often streams
     * repeat over and over (some tracks that shouldn't do this based on the flags, no real way to identify them). */
    loop_flag = !(fsbh.mode & FSOUND_LOOP_OFF); /* (fsbh.mode & FSOUND_LOOP_NORMAL) */
    /* ping-pong looping = no looping? (forward > reverse > forward) */
    VGM_ASSERT(fsbh.mode & FSOUND_LOOP_BIDI, "FSB BIDI looping found\n");

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsbh.numchannels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fsbh.deffreq;
    vgmstream->num_samples = fsbh.lengthsamples;
    vgmstream->loop_start_sample = fsbh.loopstart;
    vgmstream->loop_end_sample = fsbh.loopend;
    vgmstream->num_streams = fsbh.numsamples;
    vgmstream->meta_type = fsbh.meta_type;

    /* parse format */
    if (fsbh.mode & FSOUND_MPEG) {
        /* FSB3: ?; FSB4: Shatter, Way of the Samurai 3/4, Forza Horizon 1/2, Dragon Age Origins */
#if defined(VGM_USE_MPEG)
        mpeg_codec_data *mpeg_data = NULL;
        coding_t mpeg_coding_type;
        int fsb_padding = 0;

        //VGM_ASSERT(fsbh.mode & FSOUND_MPEG_LAYER2, "FSB FSOUND_MPEG_LAYER2 found\n");/* not always set anyway */
        VGM_ASSERT(fsbh.mode & FSOUND_IGNORETAGS, "FSB FSOUND_IGNORETAGS found\n");

        if (fsbh.flags & FMOD_FSB_SOURCE_MPEG_PADDED)
            fsb_padding = fsbh.numchannels > 2 ? 16 : 2;
        else if (fsbh.flags & FMOD_FSB_SOURCE_MPEG_PADDED4)
            fsb_padding = fsbh.numchannels > 2 ? 16 : 4;
        else /* needed by multichannel with no flags */
            fsb_padding = fsbh.numchannels > 2 ? 16 : 0;

        mpeg_data = init_mpeg_codec_data_interleaved(streamFile, start_offset, &mpeg_coding_type, vgmstream->channels, 0, fsb_padding);
        if (!mpeg_data) goto fail;
        vgmstream->codec_data = mpeg_data;
        vgmstream->coding_type = mpeg_coding_type;
        vgmstream->layout_type = layout_mpeg;

        vgmstream->interleave_block_size = mpeg_data->current_frame_size + mpeg_data->current_padding;
        //mpeg_set_error_logging(mpeg_data, 0); /* should not be needed anymore with the interleave decoder */

#elif defined(VGM_USE_FFMPEG)
        /* FFmpeg can't properly read FSB4 or FMOD's 0-padded MPEG data @ start_offset */
        goto fail;
#else
        goto fail;
#endif
    }
    else if (fsbh.mode & FSOUND_IMAADPCM) { /* (codec 0x69, Voxware Byte Aligned) */
        //VGM_ASSERT(fsbh.mode & FSOUND_IMAADPCMSTEREO, "FSB FSOUND_IMAADPCMSTEREO found\n");
        /* FSOUND_IMAADPCMSTEREO is "noninterleaved, true stereo IMA", but doesn't seem to be any different
         * (found in FSB4: Shatter, Blade Kitten (PC), Hard Corps: Uprising (PS3)) */

        /* FSB3: Bioshock (PC); FSB4: Blade Kitten (PC) */
        vgmstream->coding_type = coding_XBOX;
        vgmstream->layout_type = layout_none;
        /* "interleaved header" IMA, which seems only used with >2ch (ex. Blade Kitten 5.1) */
        if (vgmstream->channels > 2)
            vgmstream->coding_type = coding_FSB_IMA;
    }
    else if (fsbh.mode & FSOUND_VAG) {
        /* FSB1: Jurassic Park Operation Genesis
         * FSB3: ?; FSB4: Spider Man Web of Shadows, Speed Racer, Silent Hill: Shattered Memories (PS2) */

        vgmstream->coding_type = coding_PSX;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x10;
    }
    else if (fsbh.mode & FSOUND_XMA) {
        /* FSB4: Xbox360 Armored Core V, Hard Corps, Halo Anniversary */
#if defined(VGM_USE_FFMPEG)
        ffmpeg_codec_data *ffmpeg_data = NULL;
        uint8_t buf[FAKE_RIFF_BUFFER_SIZE];
        size_t bytes, block_size, block_count;
        /* not accurate but not needed by FFmpeg */
        block_size = 2048;
        block_count = fsbh.datasize / block_size; /* read_32bitLE(custom_data_offset +0x14) -1? */

        /* make a fake riff so FFmpeg can parse the XMA2 */
        bytes = ffmpeg_make_riff_xma2(buf, FAKE_RIFF_BUFFER_SIZE, fsbh.lengthsamples, fsbh.datasize, fsbh.numchannels, fsbh.deffreq, block_count, block_size);
        if (bytes <= 0)
            goto fail;

        ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,fsbh.datasize);
        if ( !ffmpeg_data ) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

#else
        goto fail;
#endif
    }
    else if (fsbh.mode & FSOUND_GCADPCM) {
        /* FSB3: ?; FSB4: de Blob (Wii), Night at the Museum, M. Night Shyamalan Avatar: The Last Airbender */

        vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->layout_type = layout_interleave_byte;
        vgmstream->interleave_block_size = 0x2;
        dsp_read_coefs_be(vgmstream, streamFile, custom_data_offset, 0x2e);
    }
    else if (fsbh.mode & FSOUND_CELT) { /* || fsbh.mode & FSOUND_OGG (same flag) */
        /* FSB4: War Thunder (PC), The Witcher 2 (PC) */

        VGM_LOG("FSB4 FSOUND_CELT found\n");
        goto fail;
    }
    else { /* PCM */
        if (fsbh.mode & FSOUND_8BITS) {
            VGM_LOG("FSB FSOUND_8BITS found\n");
            if (fsbh.mode & FSOUND_UNSIGNED) {
                vgmstream->coding_type = coding_PCM8_U; /* ? coding_PCM8_U_int */
            } else { /* FSOUND_SIGNED */
                vgmstream->coding_type = coding_PCM8; /* ? coding_PCM8_int / coding_PCM8_SB_int */
            }
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x1;
        } else {  /* Rocket Knight (PC), Another Century's Episode R (PS3), Toy Story 3 (Wii)  */
            /* sometimes FSOUND_STEREO/FSOUND_MONO is not set (ex. Dead Space iOS),
             * or only STEREO/MONO but not FSOUND_8BITS/FSOUND_16BITS is set */
            if (fsbh.flags & FMOD_FSB_SOURCE_BIGENDIANPCM) {
                vgmstream->coding_type = coding_PCM16BE;
            } else {
                vgmstream->coding_type = coding_PCM16LE; /* ? coding_PCM16LE_int ? */
            }
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
        }
    }

    /* full channel interleave, used in short streams (ex. de Blob Wii SFXs) */
    if (fsbh.numchannels > 1 && (fsbh.flags & FMOD_FSB_SOURCE_NOTINTERLEAVED)) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = fsbh.lengthcompressedbytes / fsbh.numchannels;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
