#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


typedef struct {
    int big_endian;
    int channels;
    int sample_rate;
    int codec;

    int factor;

    int loop_flag;
    int num_samples;
    int loop_start;
    int loop_end;

    int subsongs;
    int layers;

    size_t data_size;
    off_t stream_offset;
} xvag_header;

/* XVAG - Sony's Scream Tool/Stream Creator format (God of War III, Ratchet & Clank Future, The Last of Us, Uncharted) */
VGMSTREAM * init_vgmstream_xvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    xvag_header xvag = {0};
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x20;
    size_t chunk_size;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;


    /* checks */
    /* .xvag: standard
     * (extensionless): The Last Of Us (PS3) speech files */
    if (!check_extensions(streamFile,"xvag,"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
        goto fail;

    /* endian flag (XVAGs of the same game can use BE or LE, usually when reusing from other platforms) */
    xvag.big_endian = read_8bit(0x08,streamFile) & 0x01;
    if (xvag.big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    start_offset = read_32bit(0x04,streamFile);
    /* 0x08: flags? (&0x01=big endian, 0x02=?, 0x06=full RIFF AT9?)
     * 0x09: flags2? (0x00/0x01/0x04, speaker mode?)
     * 0x0a: always 0?
     * 0x0b: version-flag? (0x5f/0x60/0x61/0x62/etc) */


    /* "fmat": base format (always first) */
    if (!find_chunk(streamFile, 0x666D6174,first_offset,0, &chunk_offset,&chunk_size, xvag.big_endian, 1)) /*"fmat"*/
        goto fail;
    xvag.channels    = read_32bit(chunk_offset+0x00,streamFile);
    xvag.codec       = read_32bit(chunk_offset+0x04,streamFile);
    xvag.num_samples = read_32bit(chunk_offset+0x08,streamFile);
    /* 0x0c: samples again? */
    VGM_ASSERT(xvag.num_samples != read_32bit(chunk_offset+0x0c,streamFile), "XVAG: num_samples values don't match\n");

    xvag.factor      = read_32bit(chunk_offset+0x10,streamFile); /* for interleave */
    xvag.sample_rate = read_32bit(chunk_offset+0x14,streamFile);
    xvag.data_size = read_32bit(chunk_offset+0x18,streamFile); /* not always accurate */

    /* extra data, seen in versions 0x61+ */
    if (chunk_size > 0x1c) {
        /* number of interleaved subsongs */
        xvag.subsongs = read_32bit(chunk_offset+0x1c,streamFile);
        /* number of interleaved layers (layers * channels_per_layer = channels) */
        xvag.layers   = read_32bit(chunk_offset+0x20,streamFile);
    }
    else {
        xvag.subsongs = 1;
        xvag.layers   = 1;
    }

    total_subsongs = xvag.subsongs;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* other chunks: */
    /* "cpan": pan/volume per channel */
    /* "cues": cue/labels (rare) */
    /* "md5 ": hash (rare) */
    /* "0000": end chunk before start_offset */

    /* XVAG has no looping, but some PS3 PS-ADPCM seems to do full loops (without data flags) */
    if (xvag.codec == 0x06 && xvag.subsongs == 1) {
        size_t file_size = get_streamfile_size(streamFile);
        /* simply test if last frame is not empty = may loop */
        xvag.loop_flag = (read_8bit(file_size - 0x01, streamFile) != 0);
        xvag.loop_start = 0;
        xvag.loop_end = ps_bytes_to_samples(file_size - start_offset, xvag.channels);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(xvag.channels,xvag.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XVAG;
    vgmstream->sample_rate = xvag.sample_rate;
    vgmstream->num_samples = xvag.num_samples;
    if (xvag.loop_flag) {
        vgmstream->loop_start_sample = xvag.loop_start;
        vgmstream->loop_end_sample = xvag.loop_end;
    }
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = (xvag.data_size / total_subsongs);

    switch (xvag.codec) {
        case 0x06: /* VAG (PS-ADPCM): God of War III (PS3), Uncharted 1/2 (PS3), Ratchet and Clank Future (PS3) */
        case 0x07: /* SVAG? (PS-ADPCM with extended table?): inFamous 1 (PS3) */
            if (xvag.subsongs > 1 && xvag.layers > 1) goto fail;
            if (xvag.layers > 1 && xvag.layers != xvag.channels) goto fail;
            if (xvag.subsongs > 1 && xvag.channels > 1) goto fail; /* unknown layout */

            vgmstream->coding_type = coding_PSX;

            if (xvag.subsongs > 1) { /* God of War 3 (PS4) */
                vgmstream->layout_type = layout_blocked_xvag_subsong;
                vgmstream->interleave_block_size = 0x10;
                vgmstream->full_block_size = 0x10 * xvag.factor * xvag.subsongs;
                vgmstream->current_block_size = 0x10 * xvag.factor;
                start_offset += vgmstream->current_block_size * (target_subsong-1);
            }
            else {
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x10 * xvag.factor; /* usually 1, bigger in GoW3 PS4 */
            }
            break;

#ifdef VGM_USE_MPEG
        case 0x08: { /* MPEG: The Last of Us (PS3), Uncharted 3 (PS3), Medieval Moves (PS3) */
            mpeg_custom_config cfg = {0};

            if (xvag.subsongs > 1) goto fail;
            if (xvag.subsongs > 1 && xvag.layers > 1) goto fail;
            /* often 2ch per MPEG and rarely 1ch (GoW3 PS4) */
            if (xvag.layers > 1 && !(xvag.layers*1 == vgmstream->channels || xvag.layers*2 == vgmstream->channels)) goto fail;
            //todo rare test file in The Last of Us PS4 uses 6ch with one 2ch stream, surround MPEG/mp3pro? (decoded samples map to 6ch)

            /* "mpin": mpeg info */
            /*  0x00/04: mpeg version/layer?  other: unknown or repeats of "fmat" */
            if (!find_chunk(streamFile, 0x6D70696E,first_offset,0, &chunk_offset,NULL, xvag.big_endian, 1)) /*"mpin"*/
                goto fail;

            cfg.chunk_size = read_32bit(chunk_offset+0x1c,streamFile); /* fixed frame size */
            cfg.interleave = cfg.chunk_size * xvag.factor;

            vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_XVAG, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x09: { /* ATRAC9: Sly Cooper and the Thievius Raccoonus (Vita), The Last of Us Remastered (PS4) */
            atrac9_config cfg = {0};
            size_t frame_size;

            if (xvag.subsongs > 1 && xvag.layers > 1) goto fail;

            /* "a9in": ATRAC9 info */
            /*  0x00: frame size, 0x04: samples per frame, 0x0c: fact num_samples (no change), 0x10: encoder delay1 */
            if (!find_chunk(streamFile, 0x6139696E,first_offset,0, &chunk_offset,NULL, xvag.big_endian, 1)) /*"a9in"*/
                goto fail;

            frame_size = read_32bit(chunk_offset+0x00,streamFile);

            cfg.type = ATRAC9_XVAG;
            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(chunk_offset+0x08,streamFile);
            cfg.encoder_delay = read_32bit(chunk_offset+0x14,streamFile);

            if (xvag.subsongs > 1) {
                /* interleaves 'multiplier' superframes per subsong (all share config_data) */
                cfg.interleave_skip = frame_size * xvag.factor;
                cfg.subsong_skip = xvag.subsongs;
                /* start in subsong's first superframe */
                start_offset += (target_subsong-1) * cfg.interleave_skip * (cfg.subsong_skip-1);
            }
            else if (xvag.layers > 1) {
                /* Vita multichannel, or multilanguage [flower (Vita), Uncharted Collection (PS4)] */
                VGM_LOG("XVAG: unknown %i multistreams of size %x\n", xvag.layers, frame_size * xvag.factor);
                goto fail;//todo add
            }

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (vgmstream->layout_type == layout_blocked_xvag_subsong)
        block_update_xvag_subsong(start_offset, vgmstream);

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
