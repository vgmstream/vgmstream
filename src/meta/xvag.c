#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "xvag_streamfile.h"


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

    int target_subsong;

    size_t data_size;
    off_t stream_offset;
} xvag_header;

static int init_xvag_atrac9(STREAMFILE* sf, VGMSTREAM* vgmstream, xvag_header* xvag, off_t chunk_offset);
static layered_layout_data* build_layered_xvag(STREAMFILE* sf, xvag_header* xvag, off_t chunk_offset, off_t start_offset);

/* XVAG - Sony's Scream Tool/Stream Creator format (God of War III, Ratchet & Clank Future, The Last of Us, Uncharted) */
VGMSTREAM* init_vgmstream_xvag(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    xvag_header xvag = {0};
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x20;
    size_t chunk_size;
    int total_subsongs = 0, target_subsong = sf->stream_index;


    /* checks */
    /* .xvag: standard
     * (extensionless): The Last of Us (PS3) speech files */
    if (!check_extensions(sf,"xvag,"))
        goto fail;
    if (!is_id32be(0x00,sf, "XVAG"))
        goto fail;

    /* endian flag (XVAGs of the same game can use BE or LE, usually when reusing from other platforms) */
    xvag.big_endian = read_8bit(0x08,sf) & 0x01;
    if (xvag.big_endian) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    start_offset = read_32bit(0x04,sf);
    /* 0x08: flags? (&0x01=big endian, 0x02=?, 0x06=full RIFF AT9?)
     * 0x09: flags2? (0x00/0x01/0x04, speaker mode?)
     * 0x0a: always 0?
     * 0x0b: version-flag? (0x5f/0x60/0x61/0x62/etc) */


    /* "fmat": base format (always first) */
    if (!find_chunk(sf, 0x666D6174,first_offset,0, &chunk_offset,&chunk_size, xvag.big_endian, 1)) /*"fmat"*/
        goto fail;
    xvag.channels    = read_32bit(chunk_offset+0x00,sf);
    xvag.codec       = read_32bit(chunk_offset+0x04,sf);
    xvag.num_samples = read_32bit(chunk_offset+0x08,sf);
    /* 0x0c: samples again? */
    VGM_ASSERT(xvag.num_samples != read_32bit(chunk_offset+0x0c,sf), "XVAG: num_samples values don't match\n");

    xvag.factor      = read_32bit(chunk_offset+0x10,sf); /* for interleave */
    xvag.sample_rate = read_32bit(chunk_offset+0x14,sf);
    xvag.data_size = read_32bit(chunk_offset+0x18,sf); /* not always accurate */

    /* extra data, seen in versions 0x61+ */
    if (chunk_size > 0x1c) {
        /* number of interleaved subsongs */
        xvag.subsongs = read_32bit(chunk_offset+0x1c,sf);
        /* number of interleaved layers (layers * channels_per_layer = channels) */
        xvag.layers   = read_32bit(chunk_offset+0x20,sf);
    }
    else {
        xvag.subsongs = 1;
        xvag.layers   = 1;
    }

    total_subsongs = xvag.subsongs;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    xvag.target_subsong = target_subsong;


    /* other chunks: */
    /* "cpan": pan/volume per channel */
    /* "cues": cue/labels (rare) */
    /* "md5 ": hash (rare) */
    /* "0000": end chunk before start_offset */

    /* XVAG has no looping, but some PS3 PS-ADPCM seems to do full loops (without data flags) */
    if (xvag.codec == 0x06 && xvag.subsongs == 1) {
        size_t file_size = get_streamfile_size(sf);
        /* simply test if last frame is not empty = may loop */
        xvag.loop_flag = (read_8bit(file_size - 0x01, sf) != 0);
        xvag.loop_start = 0;
        xvag.loop_end = ps_bytes_to_samples(file_size - start_offset, xvag.channels);
    }

    /* May use 'MP3 Surround' for multichannel [Twisted Metal (PS3), The Last of Us (PS4) test file]
     * It's a mutant MP3 that decodes as 2ch but output can be routed to 6ch somehow, if manually
     * activated. Fraunhofer IIS's MP3sPlayer can do it, as can PS3 (fw v2.40+) but no others seems to.
     * So simply play as 2ch, they sound ok with slightly wider feel. No XVAG/MP3 flag exists to detect,
     * can be found in v0x60 (without layers/subsongs) and v0x61 (with them set as 1) */
    if (xvag.codec == 0x08 && xvag.channels == 6 && xvag.layers == 1) {
        xvag.channels = 2;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(xvag.channels, xvag.loop_flag);
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
        case 0x07: /* SVAG? (PS-ADPCM with extended table): inFamous 1 (PS3) */
            if (xvag.subsongs > 1 && xvag.layers > 1) goto fail;
            if (xvag.layers > 1 && xvag.layers != xvag.channels) goto fail;
            if (xvag.subsongs > 1 && xvag.channels > 1) goto fail; /* unknown layout */

            vgmstream->coding_type = coding_PSX;
            if (xvag.codec == 0x07)
                vgmstream->codec_config = 1; /* needs extended table */

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

            /* often 2ch per MPEG and rarely 1ch (GoW3 PS4) */
            if (xvag.layers > 1 && !(xvag.layers*1 == vgmstream->channels || xvag.layers*2 == vgmstream->channels)) goto fail;

            /* "mpin": mpeg info */
            if (!find_chunk(sf, 0x6D70696E,first_offset,0, &chunk_offset,NULL, xvag.big_endian, 1)) /*"mpin"*/
                goto fail;

            /* all layers/subsongs share the same config; not very useful but for posterity:
             * - 0x00: mpeg version
             * - 0x04: mpeg layer
             * - 0x08: bit rate
             * - 0x0c: sample rate
             * - 0x10: some version? (0x01-0x03)?
             * - 0x14: channels per stream?
             * - 0x18: channels per stream or total channels?
             * - 0x1c: fixed frame size (always CBR)
             * - 0x20: encoder delay (usually but not always 1201)
             * - 0x24: number of samples
             * - 0x28: some size?
             * - 0x2c: ? (0x02)
             * - 0x30: ? (0x00, 0x80)
             * - 0x34: data size
             * (rest is padding)
             * */
            cfg.chunk_size = read_32bit(chunk_offset+0x1c,sf);
            cfg.skip_samples = read_32bit(chunk_offset+0x20,sf);
            cfg.interleave = cfg.chunk_size * xvag.factor;

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_XVAG, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            /* interleaved subsongs, rarely [Sly Cooper: Thieves in Time (PS3)] */
            if (xvag.subsongs > 1) {
                temp_sf = setup_xvag_streamfile(sf, start_offset, cfg.interleave,cfg.chunk_size, (target_subsong-1), total_subsongs);
                if (!temp_sf) goto fail;
                start_offset = 0;
            }

            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x09: { /* ATRAC9: Sly Cooper and the Thievius Raccoonus (Vita), The Last of Us Remastered (PS4) */

            /* "a9in": ATRAC9 info */
            /*  0x00: frame size, 0x04: samples per frame, 0x0c: fact num_samples (no change), 0x10: encoder delay1 */
            if (!find_chunk(sf, 0x6139696E,first_offset,0, &chunk_offset,NULL, xvag.big_endian, 1)) /*"a9in"*/
                goto fail;

            if (xvag.layers > 1) {
                /* some Vita/PS4 multichannel [flower (Vita), Uncharted Collection (PS4)]. PS4 ATRAC9 also
                 * does single-stream >2ch, but this can do configs ATRAC9 can't, like 5ch/14ch/etc */
                vgmstream->layout_data = build_layered_xvag(sf, &xvag, chunk_offset, start_offset);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_ATRAC9;
                vgmstream->layout_type = layout_layered;

                break;
            }
            else {
                /* interleaved subsongs (section layers) */
                size_t frame_size = read_32bit(chunk_offset+0x00,sf);

                if (!init_xvag_atrac9(sf, vgmstream, &xvag, chunk_offset))
                    goto fail;
                temp_sf = setup_xvag_streamfile(sf, start_offset, frame_size*xvag.factor,frame_size, (target_subsong-1), total_subsongs);
                if (!temp_sf) goto fail;
                start_offset = 0;
            }

            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,temp_sf ? temp_sf : sf,start_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_ATRAC9
static int init_xvag_atrac9(STREAMFILE* sf, VGMSTREAM* vgmstream, xvag_header* xvag, off_t chunk_offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = xvag->big_endian ? read_32bitBE : read_32bitLE;
    atrac9_config cfg = {0};

    cfg.channels = vgmstream->channels;
    /* 0x00: frame size */
    /* 0x04: frame samples */
    cfg.config_data = read_32bitBE(chunk_offset+0x08,sf);
    /* 0x08: data size (layer only) */
    /* 0x10: decoder delay? */
    cfg.encoder_delay = read_32bit(chunk_offset+0x14,sf);
    /* sometimes ATRAC9 data starts with a fake RIFF, that has total channels rather than layer channels */

    vgmstream->codec_data = init_atrac9(&cfg);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_ATRAC9;
    vgmstream->layout_type = layout_none;

    return 1;
fail:
    return 0;
}
#endif

static layered_layout_data* build_layered_xvag(STREAMFILE* sf, xvag_header* xvag, off_t chunk_offset, off_t start_offset) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = xvag->big_endian ? read_32bitBE : read_32bitLE;
    int i, layers = xvag->layers;
    int chunk, chunks = layers * xvag->subsongs;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* interleaves frames per substreams */
    for (i = 0; i < layers; i++) {
        int layer_channels = xvag->channels / layers; /* all streams must be equal (XVAG limitation) */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, xvag->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = xvag->sample_rate;
        data->layers[i]->num_samples = xvag->num_samples;

        switch(xvag->codec) {
#ifdef VGM_USE_ATRAC9
            case 0x09: {
                size_t frame_size = read_32bit(chunk_offset+0x00,sf);

                if (!init_xvag_atrac9(sf, data->layers[i], xvag, chunk_offset))
                    goto fail;

                /* interleaves N layers for custom multichannel, may rarely use subsongs [Days Gone (PS4) multilayer test]
                 * ex. 2 layers, 1 subsong : [L1][L2][L1][L2]
                 * ex. 2 layers, 2 subsongs: [L1S1][L2S1][L1S2][L2S2] (assumed, could be [L1S1][L1S2][L2S1][L2S2]) */
                chunk = i + xvag->subsongs * (xvag->target_subsong - 1); /* [L1S1][L2S1][L1S2][L2S2] */
              //chunk = i * xvag->subsongs + (xvag->target_subsong - 1); /* [L1S1][L1S2][L2S1][L2S2] */

                temp_sf = setup_xvag_streamfile(sf, start_offset, frame_size*xvag->factor, frame_size, chunk, chunks);
                if (!temp_sf) goto fail;
                break;
            }
#endif
            default:
                goto fail;
        }

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
