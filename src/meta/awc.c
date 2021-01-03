#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "awc_xma_streamfile.h"

typedef struct {
    int big_endian;
    int is_encrypted;
    int is_music;

    int total_subsongs;

    int channels;
    int sample_rate;
    int codec;
    int num_samples;

    int block_chunk;

    off_t stream_offset;
    size_t stream_size;
    off_t vorbis_offset[VGMSTREAM_MAX_CHANNELS];

} awc_header;

static int parse_awc_header(STREAMFILE* sf, awc_header* awc);

static layered_layout_data* build_layered_awc(STREAMFILE* sf, awc_header* awc);


/* AWC - from RAGE (Rockstar Advanced Game Engine) audio [Red Dead Redemption, Max Payne 3, GTA5 (multi)] */
VGMSTREAM* init_vgmstream_awc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    awc_header awc = {0};


    /* checks */
    if (!check_extensions(sf,"awc"))
        goto fail;
    if (!parse_awc_header(sf, &awc))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(awc.channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = awc.sample_rate;
    vgmstream->num_samples = awc.num_samples;
    vgmstream->num_streams = awc.total_subsongs;
    vgmstream->stream_size = awc.stream_size;
    vgmstream->meta_type = meta_AWC;


    switch(awc.codec) {
        case 0x00:      /* PCM (PC) sfx, very rare, lower sample rates? [Max Payne 3 (PC)] */
        case 0x01:      /* PCM (PC/PS3) sfx, rarely */
            if (awc.is_music) goto fail; /* blocked_awc needs to be prepared */
            vgmstream->coding_type = awc.big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x04:      /* IMA (PC) */
            vgmstream->coding_type = coding_AWC_IMA;
            vgmstream->layout_type = awc.is_music ? layout_blocked_awc : layout_none;
            vgmstream->full_block_size = awc.block_chunk;
            vgmstream->codec_endian = awc.big_endian;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x05: {    /* XMA2 (X360) */
            uint8_t buf[0x100];
            size_t bytes, block_size, block_count, substream_size;
            off_t substream_offset;

            if (awc.is_music) {
                /* 1ch XMAs in blocks, we'll use layered layout + custom IO to get multi-FFmpegs working */
                int i;
                layered_layout_data * data = NULL;

                /* init layout */
                data = init_layout_layered(awc.channels);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_FFmpeg;

                /* open each layer subfile */
                for (i = 0; i < awc.channels; i++) {
                    STREAMFILE* temp_sf = NULL;
                    int layer_channels = 1;

                    /* build the layer VGMSTREAM */
                    data->layers[i] = allocate_vgmstream(layer_channels, 0);
                    if (!data->layers[i]) goto fail;

                    data->layers[i]->sample_rate = awc.sample_rate;
                    data->layers[i]->meta_type = meta_AWC;
                    data->layers[i]->coding_type = coding_FFmpeg;
                    data->layers[i]->layout_type = layout_none;
                    data->layers[i]->num_samples = awc.num_samples;

                    /* setup custom IO streamfile, pass to FFmpeg and hope it's fooled */
                    temp_sf = setup_awc_xma_streamfile(sf, awc.stream_offset, awc.stream_size, awc.block_chunk, awc.channels, i);
                    if (!temp_sf) goto fail;

                    substream_offset = 0; /* where FFmpeg thinks data starts, which our custom sf will clamp */
                    substream_size = get_streamfile_size(temp_sf); /* data of one XMA substream without blocks */
                    block_size = 0x8000; /* no idea */
                    block_count = substream_size / block_size; /* not accurate but not needed */

                    bytes = ffmpeg_make_riff_xma2(buf, 0x100, awc.num_samples, substream_size, layer_channels, awc.sample_rate, block_count, block_size);
                    data->layers[i]->codec_data = init_ffmpeg_header_offset(temp_sf, buf,bytes, substream_offset,substream_size);

                    xma_fix_raw_samples(data->layers[i], temp_sf, substream_offset,substream_size, 0, 0,0); /* samples are ok? */

                    close_streamfile(temp_sf);
                    if (!data->layers[i]->codec_data) goto fail;
                }

                /* setup layered VGMSTREAMs */
                if (!setup_layout_layered(data))
                    goto fail;
            }
            else {
                /* regular XMA for sfx */
                block_size = 0x8000; /* no idea */
                block_count = awc.stream_size / block_size; /* not accurate but not needed */

                bytes = ffmpeg_make_riff_xma2(buf, 0x100, awc.num_samples, awc.stream_size, awc.channels, awc.sample_rate, block_count, block_size);
                vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, awc.stream_offset,awc.stream_size);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;

                xma_fix_raw_samples(vgmstream, sf, awc.stream_offset,awc.stream_size, 0, 0,0); /* samples are ok? */
            }

            break;
        }


#endif
#ifdef VGM_USE_MPEG
        case 0x07: {    /* MPEG (PS3) */
            mpeg_custom_config cfg = {0};

            cfg.chunk_size = awc.block_chunk;
            cfg.big_endian = awc.big_endian;

            vgmstream->codec_data = init_mpeg_custom(sf, awc.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_AWC, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case 0x08: {   /* Vorbis (PC) [Red Dead Redemption 2 (PC)] */
            if (awc.is_music) {
                vgmstream->layout_data = build_layered_awc(sf, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_VORBIS_custom;
            }
            else {
                vorbis_custom_config cfg = {0};

                cfg.channels = awc.channels;
                cfg.sample_rate = awc.sample_rate;
                cfg.header_offset = awc.vorbis_offset[0];

                vgmstream->codec_data = init_vorbis_custom(sf, awc.stream_offset, VORBIS_AWC, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
                vgmstream->coding_type = coding_VORBIS_custom;
            }
            break;
        }
#endif
        default:
            VGM_LOG("AWC: unknown codec 0x%02x\n", awc.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, awc.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* Parse Rockstar's AWC header (much info from LibertyV: https://github.com/koolkdev/libertyv).
 * Made of entries for N streams, each with a number of tags pointing to chunks (header, data, events, etc). */
static int parse_awc_header(STREAMFILE* sf, awc_header* awc) {
    uint64_t (*read_u64)(off_t,STREAMFILE*) = NULL;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = NULL;
    int i, ch, entries;
    uint32_t flags, info_header, tag_count = 0, tags_skip = 0;
    off_t offset;
    int target_subsong = sf->stream_index;


    /* check header */
    if (read_u32be(0x00,sf) != 0x41444154 &&  /* "ADAT" (LE) */
        read_u32be(0x00,sf) != 0x54414441)    /* "TADA" (BE) */
        goto fail;

    awc->big_endian = read_u32be(0x00,sf) == 0x54414441;
    if (awc->big_endian) {
        read_u64 = read_u64be;
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        read_u64 = read_u64le;
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }


    flags = read_u32(0x04,sf);
    entries = read_u32(0x08,sf);
    //header_size = read_u32(0x0c,sf); /* after to stream id/tags, not including chunks */

    offset = 0x10;

    if ((flags & 0xFF00FFFF) != 0xFF000001 || (flags & 0x00F00000)) {
        VGM_LOG("AWC: unknown flags 0x%08x\n", flags);
        goto fail;
    }

    if (flags & 0x00010000) /* some kind of mini offset table */
        offset += 0x2 * entries;
    //if (flags % 0x00020000) /* seems to indicate chunks are not ordered (ie. header may go after data) */
    //  ...
    //if (flags % 0x00040000) /* music/multichannel flag? (GTA5, not seen in RDR) */
    //  awc->is_music = 1;
    if (flags & 0x00080000) /* encrypted data chunk (most of GTA5 PC) */
        awc->is_encrypted = 1;

    if (awc->is_encrypted) {
        VGM_LOG("AWC: encrypted data found\n");
        goto fail;
    }

    /* Music when the first id is 0 (base/fake entry with info for all channels), sfx pack otherwise.
     * sfx = N single streams, music = N-1 interleaved mono channels (even for MP3/XMA).
     * Music seems layered (N-1/2 stereo pairs), maybe set with events? */
    awc->is_music = (read_u32(offset + 0x00,sf) & 0x1FFFFFFF) == 0x00000000;
    if (awc->is_music) { /* all streams except id 0 is a channel */
        awc->total_subsongs = 1;
        target_subsong = 1; /* we only need id 0, though channels may have its own tags/chunks */
    }
    else { /* each stream is a single sound */
        awc->total_subsongs = entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > awc->total_subsongs || awc->total_subsongs < 1) goto fail;
    }


    /* get stream base info */
    for (i = 0; i < entries; i++) {
        info_header = read_u32(offset + 0x04*i, sf);
        tag_count   = (info_header >> 29) & 0x7; /* 3b */
        //id        = (info_header >>  0) & 0x1FFFFFFF; /* 29b */
        if (target_subsong-1 == i)
            break;
        tags_skip += tag_count; /* tags to skip to reach target's tags, in the next header */
    }
    offset += 0x04*entries;
    offset += 0x08*tags_skip;

    /* get stream tags */
    for (i = 0; i < tag_count; i++) {
        uint64_t tag_header;
        uint8_t tag_type;
        size_t tag_size;
        off_t tag_offset;

        tag_header = read_u64(offset + 0x08*i,sf);
        tag_type   = (uint8_t)((tag_header >> 56) & 0xFF); /* 8b */
        tag_size   =  (size_t)((tag_header >> 28) & 0x0FFFFFFF); /* 28b */
        tag_offset =   (off_t)((tag_header >>  0) & 0x0FFFFFFF); /* 28b */
        ;VGM_LOG("AWC: tag%i/%i at %lx: t=%x, o=%lx, s=%x\n", i, tag_count, offset + 0x08*i, tag_type, tag_offset, tag_size);

        /* Tags are apparently part of a hash derived from a word ("data", "format", etc).
         * If music + 1ch, the header and data chunks can repeat for no reason (sometimes not even pointed). */
        switch(tag_type) {
            case 0x55: /* data */
                awc->stream_offset = tag_offset;
                awc->stream_size = tag_size;
                break;

            case 0x48: /* music header */

                if (!awc->is_music) {
                    VGM_LOG("AWC: music header found in sfx\n");
                    goto fail;
                }

                /* 0x00(32): unknown (some count?) */
                awc->block_chunk = read_u32(tag_offset + 0x04,sf);
                awc->channels = read_u32(tag_offset + 0x08,sf);

                if (awc->channels != entries - 1) { /* not counting id-0 */
                    VGM_LOG("AWC: number of music channels doesn't match entries\n");
                    goto fail;
                }

                for (ch = 0; ch < awc->channels; ch++) {
                    int num_samples, sample_rate, codec;
                    /* 0x00): stream id (not always in the header entries order) */
                    num_samples = read_u32(tag_offset + 0x0c + 0x10*ch + 0x04,sf);
                    /* 0x08: headroom */
                    sample_rate = read_u16(tag_offset + 0x0c + 0x10*ch + 0x0a,sf);
                    codec = read_u8(tag_offset + 0x0c + 0x10*ch + 0x0c,sf);
                    /* 0x0d(8): round size? */
                    /* 0x0e: unknown (zero/-1) */

                    /* validate channels differences */
                    if ((awc->num_samples && !(awc->num_samples >= num_samples - 10 && awc->num_samples <= num_samples + 10)) ||
                        (awc->sample_rate && awc->sample_rate != sample_rate)) {
                        VGM_LOG("AWC: found header diffs in channel %i, ns=%i vs %i, sr=%i vs %i\n",
                                ch, awc->num_samples, num_samples, awc->sample_rate, sample_rate);
                        /* sometimes (often cutscenes in Max Payne 3 and RDR DLC) channels have sample diffs,
                         * probably one stream is simply silent after its samples end */
                    }

                    if ((awc->codec && awc->codec != codec)) {
                        VGM_LOG("AWC: found header diffs in channel %i, c=%i vs %i\n", ch, awc->codec, codec);
                        goto fail;
                    }

                    if (awc->num_samples < num_samples) /* use biggest channel */
                        awc->num_samples = num_samples;
                    awc->sample_rate = sample_rate;
                    awc->codec = codec;
                }

                break;

            case 0xFA: /* sfx header */
                if (awc->is_music) {
                    VGM_LOG("AWC: sfx header found in music\n");
                    goto fail;
                }

                awc->num_samples = read_u32(tag_offset + 0x00,sf);
                /* 0x04: -1? */
                awc->sample_rate = read_u16(tag_offset + 0x08,sf);
                /* 0x0a: unknown x4 */
                /* 0x12: null? */
                awc->codec = read_u8(tag_offset + 0x13, sf);
                awc->channels = 1;
                break;

            case 0x76: /* sfx header for vorbis */
                if (awc->is_music) {
                    VGM_LOG("AWC: sfx header found in music\n");
                    goto fail;
                }

                awc->num_samples = read_u32(tag_offset + 0x00,sf);
                /* 0x04: -1? */
                awc->sample_rate = read_u16(tag_offset + 0x08,sf);
                /* 0x0a: granule start? (negative) */
                /* 0x0c: granule max? */
                /* 0x10: unknown */
                awc->codec = read_u8(tag_offset + 0x1c, sf); /* 16b? */
                /* 0x1e: vorbis header size */
                awc->channels = 1;

                awc->vorbis_offset[0] = tag_offset + 0x20;
                break;

            case 0xA3: /* block-to-sample table (32b x number of blocks w/ num_samples at the start of each block) */
            case 0xBD: /* events (32bx4): type_hash, params_hash, timestamp_ms, flags */
            case 0x5C: /* animation/RSC config? */
            default:   /* 0x68=midi?, 0x36=hash thing?, 0x2B=sizes, 0x5A/0xD9=? */
                //VGM_LOG("AWC: ignoring unknown tag 0x%02x\n", tag);
                break;
        }
    }

    if (!awc->stream_offset) {
        VGM_LOG("AWC: stream offset not found\n");
        goto fail;
    }

    /* vorbis offset table, somehow offsets are unordered and can go before tags */
    if (awc->is_music && awc->codec == 0x08) {
        offset += 0x08 * tag_count;

        for (ch = 0; ch < awc->channels; ch++) {
            awc->vorbis_offset[ch] = read_u16(offset + 0x08*ch + 0x00, sf);
            /* 0x02: always 0xB000? */
            /* 0x04: always 0x00CD? */
            /* 0x06: always 0x7F00? */
        }
    }


    /* In music mode, data is divided into blocks of block_chunk size with padding.
     * Each block has a header/seek table and interleaved data for all channels */
    {
        int32_t seek_start = read_u32(awc->stream_offset, sf); /* -1 in later (RDR2) versions */
        if (awc->is_music && !(seek_start == 0 || seek_start == -1)) {
            VGM_LOG("AWC: music found, but block doesn't start with seek table at %x\n", (uint32_t)awc->stream_offset);
            goto fail;
        }
    }


    return 1;
fail:
    return 0;
}

/* ************************************************************************* */

//TODO: this method won't work properly, needs internal handling of blocks.
//
// This setups a decoder per block, but seems Vorbis' uses first frame as setup so it
// returns samples (576 vs 1024), making num_samples count in each block being off + causing
// gaps. So they must be using a single encoder + setting decode_to_discard per block
// to ge the thing working.
//
// However since blocks are probably also used for seeking, maybe they aren't resetting
// the decoder when seeking? or they force first frame to be 1024?
//
// In case of Vorvis, when setting skip samples seems repeated data from last block is
// exactly last 0x800 bytes of that channel.

static VGMSTREAM* build_block_vgmstream(STREAMFILE* sf, awc_header* awc, int channel, int32_t num_samples, int32_t skip_samples, off_t block_start, size_t block_size) {
    STREAMFILE* temp_sf = NULL;
    VGMSTREAM* vgmstream = NULL;
    int block_channels = 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(block_channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = awc->sample_rate;
    vgmstream->num_samples = num_samples - skip_samples;
    vgmstream->stream_size = block_size;
    vgmstream->meta_type = meta_AWC;

    switch(awc->codec) {
#ifdef VGM_USE_VORBIS
        case 0x08: {   /* Vorbis (PC) [Red Dead Redemption 2 (PC)] */
            vorbis_custom_config cfg = {0};

            cfg.channels = 1;
            cfg.sample_rate = awc->sample_rate;
            cfg.header_offset = awc->vorbis_offset[channel];
            //cfg.skip_samples = skip_samples; //todo

            vgmstream->codec_data = init_vorbis_custom(sf, block_start, VORBIS_AWC, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
        }
        break;
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, block_start))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* build_blocks_vgmstream(STREAMFILE* sf, awc_header* awc, int channel) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;
    int i, ch;
    int blocks = awc->stream_size / awc->block_chunk + (awc->stream_size % awc->block_chunk ? 1 : 0) ;


    /* init layout */
    data = init_layout_segmented(blocks);
    if (!data) goto fail;

    /* one segment per block of this channel */
    for (i = 0; i < blocks; i++) {
        off_t block_offset = awc->stream_offset + i * awc->block_chunk;
        int32_t num_samples = 0, skip_samples = 0;
        uint32_t header_skip = 0, block_skip = 0, block_start = 0, block_data = 0;

        /* read stupid block crap to get proper offsets and whatnot, format:
         * - per channel: number of channel entries + skip samples + num samples
         * - per channel: seek table with N entries */
        for (ch = 0; ch < awc->channels; ch++) {
            /* 0x00: -1 */
            int entries             = read_u32le(block_offset + 0x18 * ch + 0x04, sf);
            int32_t entry_skip      = read_u32le(block_offset + 0x18 * ch + 0x08, sf);
            int32_t entry_samples   = read_u32le(block_offset + 0x18 * ch + 0x0c, sf);

            if (ch == channel) {
                num_samples = entry_samples;
                skip_samples = entry_skip;

                block_start = block_offset + block_skip;
                block_data = entries * 0x800;
            }

            header_skip += 0x18 + entries * 0x04;
            block_skip += entries * 0x800;
        }

        if (!block_start)
            goto fail;

        header_skip = align_size_to_block(header_skip, 0x800);
        block_start += header_skip;
        //;VGM_LOG("AWC: build ch%i, block=%i at %lx, o=%x, s=%x, ns=%i, ss=%i\n", channel, i, block_offset, block_start, block_data, num_samples, skip_samples);

        data->segments[i] = build_block_vgmstream(sf, awc, channel, num_samples, skip_samples, block_start, block_data);
        if (!data->segments[i]) goto fail;
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;

    /* build the layout VGMSTREAM */
    vgmstream = allocate_segmented_vgmstream(data, 0, 0, 0);
    if (!vgmstream) goto fail;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_segmented(data);
    return NULL;
}

/* ************************************************************************* */

/* Make layers per channel for AWC's abhorrent blocks.
 *
 * File has N channels = N streams, that use their own mono decoder.
 * Each block then has header + seek table for all channels. But in each block there is
 * a "skip samples" value per channel, and blocks repeat some data from last block
 * for this, so PCM must be discarded. Also, channels in a block don't need to have
 * the same number of samples.
 */
static layered_layout_data* build_layered_awc(STREAMFILE* sf, awc_header* awc) {
    int i;
    layered_layout_data* data = NULL;


    /* init layout */
    data = init_layout_layered(awc->channels);
    if (!data) goto fail;

    /* open each layer subfile */
    for (i = 0; i < awc->channels; i++) {
        data->layers[i] = build_blocks_vgmstream(sf, awc, i);
        if (!data->layers[i]) goto fail;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    return data;
fail:
    free_layout_layered(data);
    return NULL;
}

