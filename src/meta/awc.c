#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "awc_xma_streamfile.h"


#define AWC_MAX_MUSIC_CHANNELS 20

typedef struct {
    int big_endian;
    int is_encrypted;
    int is_streamed; /* implicit: streams=music, sfx=memory */

    int total_subsongs;

    int channels;
    int sample_rate;
    int codec;
    int num_samples;

    int block_count;
    int block_chunk;

    off_t stream_offset;
    size_t stream_size;
    off_t vorbis_offset[VGMSTREAM_MAX_CHANNELS];

} awc_header;

static int parse_awc_header(STREAMFILE* sf, awc_header* awc);

static layered_layout_data* build_layered_awc(STREAMFILE* sf, awc_header* awc);


/* AWC - Audio Wave Container from RAGE (Rockstar Advanced Game Engine) [Red Dead Redemption, Max Payne 3, GTA5 (multi)] */
VGMSTREAM* init_vgmstream_awc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    awc_header awc = {0};


    /* checks */
    if (!parse_awc_header(sf, &awc))
        return NULL;
    if (!check_extensions(sf,"awc"))
        return NULL;


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
            if (awc.is_streamed) goto fail; /* blocked_awc needs to be prepared */
            vgmstream->coding_type = awc.big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x04:      /* IMA (PC) */
            vgmstream->coding_type = coding_AWC_IMA;
            vgmstream->layout_type = awc.is_streamed ? layout_blocked_awc : layout_none;
            vgmstream->full_block_size = awc.block_chunk;
            vgmstream->codec_endian = awc.big_endian;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x05: {    /* XMA2 (X360) */
            uint32_t substream_size, substream_offset;

            if (awc.is_streamed) {
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

                    data->layers[i]->meta_type = meta_AWC;
                    data->layers[i]->coding_type = coding_FFmpeg;
                    data->layers[i]->layout_type = layout_none;
                    data->layers[i]->sample_rate = awc.sample_rate;
                    data->layers[i]->num_samples = awc.num_samples;

                    /* setup custom IO streamfile, pass to FFmpeg and hope it's fooled */
                    temp_sf = setup_awc_xma_streamfile(sf, awc.stream_offset, awc.stream_size, awc.block_chunk, awc.channels, i);
                    if (!temp_sf) goto fail;

                    substream_offset = 0x00; /* where FFmpeg thinks data starts, which our custom sf will clamp */
                    substream_size = get_streamfile_size(temp_sf); /* data of one XMA substream without blocks */

                    data->layers[i]->codec_data = init_ffmpeg_xma2_raw(temp_sf, substream_offset, substream_size, awc.num_samples, layer_channels, awc.sample_rate, 0, 0);
                    if (data->layers[i])
                        xma_fix_raw_samples(data->layers[i], temp_sf, substream_offset, substream_size, 0, 0,0); /* samples are ok? */
                    close_streamfile(temp_sf);
                    if (!data->layers[i]->codec_data) goto fail;
                }

                /* setup layered VGMSTREAMs */
                if (!setup_layout_layered(data))
                    goto fail;
            }
            else {
                /* regular XMA for sfx */
                vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, awc.stream_offset, awc.stream_size, awc.num_samples, awc.channels, awc.sample_rate, 0, 0);
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
            if (awc.is_streamed) {
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

#ifdef VGM_USE_ATRAC9
        case 0x0F: {   /* ATRAC9 (PC) [Red Dead Redemption (PS4)] */
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_ATRAC9;
            }
            else {
                VGMSTREAM* temp_vs = NULL;
                STREAMFILE* temp_sf = NULL;

                temp_sf = setup_subfile_streamfile(sf, awc.stream_offset, awc.stream_size, "at9");
                if (!temp_sf) goto fail;

                temp_vs = init_vgmstream_riff(temp_sf);
                close_streamfile(temp_sf);
                if (!temp_vs) goto fail;

                temp_vs->num_streams = vgmstream->num_streams;
                temp_vs->stream_size = vgmstream->stream_size;
                temp_vs->meta_type = vgmstream->meta_type;
                strcpy(temp_vs->stream_name, vgmstream->stream_name);

                close_vgmstream(vgmstream);
                //vgmstream = temp_vs;
                return temp_vs;
            }
            break;
        }
#endif

        case 0x0C:      /* DSP-sfx (Switch) */
        case 0x10:      /* DSP-music (Switch) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = awc.is_streamed ? layout_blocked_awc : layout_none;
            vgmstream->full_block_size = awc.block_chunk;

            if (!awc.is_streamed) {
                /* dsp header */
                dsp_read_coefs_le(vgmstream, sf, awc.stream_offset + 0x1c + 0x00, 0x00);
                dsp_read_hist_le (vgmstream, sf, awc.stream_offset + 0x1c + 0x20, 0x00);
                awc.stream_offset += 0x60;

                /* shouldn't be possible since it's only used for sfx anyway */
                if (awc.channels > 1)
                    goto fail;
            } 
            break;

        case 0xFF:
            vgmstream->coding_type = coding_SILENCE;
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "[%s]", "midi");
            break;

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
 *
 * AWC defines logical streams/tracks, each with N tags (type+offset+size) that point to headers/tables with info.
 * First stream may be a "music" type, then other streams are used as channels and not always define tags.
 * When the "stream" flag is set data is divided into "blocks" (used for music), described later.
 * Streams are ordered by hash/id and its tags go in order, but data may be unordered (1st stream audio
 * or headers could go after others). Defined streams also may be unused/dummy.
 * Hashes are actually reversable and more or less stream names (see other tools).
 *
 * Rough file format:
 * - base header
 * - stream tag starts [optional]
 * - stream hash ids and tag counts (stream N has M tags)
 * - tags per stream
 * - data from tags (headers, tables, audio data, etc)
 */
static int parse_awc_header(STREAMFILE* sf, awc_header* awc) {
    uint64_t (*read_u64)(off_t,STREAMFILE*) = NULL; //TODO endian
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = NULL;
    int entries;
    uint32_t flags, tag_count = 0, tags_skip = 0;
    uint32_t offset;
    int target_subsong = sf->stream_index;

    /** base header **/
    if (is_id32be(0x00,sf,"ADAT")) {
        awc->big_endian = false;
    }
    else if (is_id32be(0x00,sf,"TADA")) {
        awc->big_endian = true;
    }
    else {
        return false;
    }

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
    //header_size = read_u32(0x0c,sf); /* after stream id+tags */

    offset = 0x10;

    /* flags = 8b (always FF) + 8b (actual flags) + 16b (version, 00=rarely, 01=common) */
    if ((flags & 0xFF00FFFF) != 0xFF000001 || (flags & 0x00F00000)) {
        VGM_LOG("AWC: unknown flags 0x%08x\n", flags);
        goto fail;
    }

    /* stream tag starts (ex. stream#0 = 0, stream#1 = 4, stream#2 = 7: to read tags from stream#2 skip to 7th tag) */
    if (flags & 0x00010000)
        offset += 0x2 * entries;

    /* seems to indicate chunks are not ordered (ie. header structures from tags may go after data), usually for non-streams */
    //if (flags % 0x00020000)
    //  awc->is_unordered = 1;

    /* stream/multichannel flag (GTA5 only) */
    //if (flags % 0x00040000)
    //  awc->is_multichannel = 1;

    /* encrypted data chunk (most of GTA5 PC for licensed audio) */
    if (flags & 0x00080000)
        awc->is_encrypted = 1;

    if (awc->is_encrypted) {
        VGM_LOG("AWC: encrypted data found\n");
        goto fail;
    }


    /* When first stream hash/id is 0 AWC it has fake entry with info for all channels = music, sfx pack otherwise.
     * sfx = N single streams, music = N interleaved mono channels (even for MP3/XMA/Vorbis/etc).
     * Channels set a stream hash/id that typically is one of the defined ones and its tags do apply to that
     * channel, but rarely may not exist. Ex.:
     * 
     * - bgm01.awc
     * Stream ID 00000000 (implicit: music stream, all others aren't used)
     *   Tag: music header
     *      Channel 0: ID 9d66fe4c
     *      Channel 1: ID 7a3837ef
     *      Channel 2: ID 032c57e9 (not actually defined)
     *   Tag: data chunk
     *  #Tag: sfx header (only in buggy files)
     * Stream ID 7a3837ef (no tags)
     * Stream ID 9d66fe4c (notice this is channel 0 but streams are ordered by hash)
     *   Tag: Event config
     *
     * - sfx01.awc
     * Stream ID 9d66fe4c
     *   Tag: sfx header
     *   Tag: data chunk
     * Stream ID 7a3837ef
     *   Tag: sfx header
     *   Tag: data chunk
     *
     * Music 'stream' defines it's own (streamed/blocked) data chunk, so other stream's data or headers aren't used,
     * but in rare cases they actually define a useless sfx header or even a separate cloned data chunk. That seems
     * to be a bug and are ignored (ex. RDR's ftr_harmonica_01, or RDR SW's countdown_song_01).
     */

    awc->is_streamed = (read_u32(offset + 0x00,sf) & 0x1FFFFFFF) == 0x00000000; /* first stream's hash/id is 0 */
    if (awc->is_streamed) { /* music with N channels, other streams aren't used ignored */
        awc->total_subsongs = 1;
        target_subsong = 1;
    }
    else { /* sfx pack, each stream is a sound */
        awc->total_subsongs = entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > awc->total_subsongs || awc->total_subsongs < 1) goto fail;
    }


    /** stream ids and tag counts **/
    for (int i = 0; i < entries; i++) {
        uint32_t info_header = read_u32(offset + 0x04*i, sf);
        tag_count   = (info_header >> 29) & 0x7; /* 3b */
        //hash_id   = (info_header >>  0) & 0x1FFFFFFF; /* 29b */
        if (target_subsong - 1 == i)
            break;
        tags_skip += tag_count; /* tags to skip to reach target's tags, in the next header */
    }
    offset += 0x04 * entries;
    offset += 0x08 * tags_skip;


    /** tags per stream **/
    for (int i = 0; i < tag_count; i++) {
        uint64_t tag_header = read_u64(offset + 0x08*i,sf);
        uint8_t  tag_type   = ((tag_header >> 56) & 0xFF); /* 8b */
        uint32_t tag_size   = ((tag_header >> 28) & 0x0FFFFFFF); /* 28b */
        uint32_t tag_offset = ((tag_header >>  0) & 0x0FFFFFFF); /* 28b */
        //;VGM_LOG("AWC: tag %i/%i at %x: t=%x, o=%x, s=%x\n", i+1, tag_count, offset + 0x08*i, tag_type, tag_offset, tag_size);

        /* types are apparently part of a hash derived from a word ("data", "format", etc). */
        switch(tag_type) {
            case 0x55: /* data */
                awc->stream_offset = tag_offset;
                awc->stream_size = tag_size;
                break;

            case 0x48: /* music header */
                if (!awc->is_streamed) {
                    VGM_LOG("AWC: music header found but not streamed\n");
                    goto fail;
                }

                awc->block_count = read_u32(tag_offset + 0x00,sf);
                awc->block_chunk = read_u32(tag_offset + 0x04,sf);
                awc->channels    = read_u32(tag_offset + 0x08,sf);

                if (awc->channels != entries - 1) { /* not counting id-0 */
                    VGM_LOG("AWC: number of music channels doesn't match entries\n");
                    goto fail;
                }

                for (int ch = 0; ch < awc->channels; ch++) {
                    int num_samples, sample_rate, codec;

                    /* 0x00: reference stream hash/id  */
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
                if (awc->is_streamed) {
                    VGM_LOG("AWC: sfx header found but streamed\n");
                    break; //goto fail; /* rare (RDR PC/Switch) */
                }

                awc->num_samples = read_u32(tag_offset + 0x00,sf);
                /* 0x04: -1? */
                awc->sample_rate = read_u16(tag_offset + 0x08,sf);
                /* 0x0a: headroom */
                /* 0x0c: unknown */
                /* 0x0e: unknown */
                /* 0x10: unknown */
                /* 0x12: null? */
                awc->codec = read_u8(tag_offset + 0x13, sf);
                /* 0x14: ? (PS3 only, for any codec) */

                awc->channels = 1;
                break;

            case 0x76: /* sfx header for vorbis */
                if (awc->is_streamed) {
                    VGM_LOG("AWC: sfx header found but streamed\n");
                    goto fail;
                }

                awc->num_samples = read_u32(tag_offset + 0x00,sf);
                /* 0x04: -1? */
                awc->sample_rate = read_u16(tag_offset + 0x08,sf);
                /* 0x0a: headroom */
                /* 0x0c: unknown */
                /* 0x0e: unknown */
                /* 0x10: unknown */
                awc->codec = read_u8(tag_offset + 0x1c, sf); /* 16b? */
                /* 0x1e: vorbis setup size */
                awc->vorbis_offset[0] = tag_offset + 0x20; /* data up to vorbis setup size */

                awc->channels = 1;
                break;

            case 0x68: /* midi data [Red Dead Redemption 2 (PC)] */
                /* set fake info so awc doesn't break */
                awc->stream_offset = tag_offset;
                awc->stream_size = tag_size;

                awc->num_samples = 48000;
                awc->sample_rate = 48000;
                awc->codec = 0xFF;
                awc->channels = 1;
                break;

            case 0xA3: /* block-to-sample table (32b x number of blocks w/ num_samples at the start of each block)
                        * or frame-size table (16b x number of frames) in some cases (ex. sfx+mpeg but not sfx+vorbis) */
            case 0xBD: /* events (32bx4): type_hash, params_hash, timestamp_ms, flags */
            case 0x5C: /* animation/RSC info? */
            case 0x81: /* animation/CSR info? */
            case 0x36: /* list of hash-things? */
            case 0x2B: /* events/sizes? */
            case 0x7f: /* vorbis setup (for streams) */
            default:   /* 0x68=midi?, 0x5A/0xD9=? */
                //VGM_LOG("AWC: ignoring unknown tag 0x%02x\n", tag);
                break;
        }
    }

    /* in music mode there tags for other streams we don't need, except for vorbis that have one setup packet */
    //TODO not correct (assumes 1 tag per stream and channel order doesn't match stream order)
    // would need to read N tags and match channel id<>stream id, all vorbis setups are the same though)
    if (awc->is_streamed && awc->codec == 0x08) {
        offset += 0x08 * tag_count;

        for (int ch = 0; ch < awc->channels; ch++) {
            awc->vorbis_offset[ch] = read_u16(offset + 0x08*ch + 0x00, sf); /* tag offset */
        }
    }

    if (!awc->stream_offset) {
        VGM_LOG("AWC: stream offset not found\n");
        goto fail;
    }

    return 1;
fail:
    return 0;
}

/* ************************************************************************* */

typedef struct {
    int start_entry;
    int entries;
    int32_t channel_skip;
    int32_t channel_samples;

    uint32_t extradata;

    /* derived */
    uint32_t chunk_start;
    uint32_t chunk_size;
} awc_block_t;

typedef struct {
    awc_block_t blk[AWC_MAX_MUSIC_CHANNELS];
} awc_block_info_t;

/* Block format:
 * - block header for all channels (needed to find frame start)
 * - frames from channel 1
 * - ...
 * - frames from channel N
 * - usually there is padding between channels or blocks (usually 0s but seen 0x97 in AT9)
 * 
 * Header format:
 * - per channel (frame start table)
 *   0x00: start entry for that channel? (-1 in vorbis)
 *         may be off by +1/+2?
 *         ex. on block 0, ch0/1 have 0x007F frames, a start entry is: ch0=0x0000, ch1=0x007F (MP3)
 *         ex. on block 0, ch0/1 have 0x02A9 frames, a start entry is: ch0=0x0000, ch1=0x02AA (AT9) !!
 *         (sum of all values from all channels may go beyond all posible frames, no idea)
 *   0x04: frames in this channel (may be different between channels)
 *         'frames' here may be actual single decoder frames or a chunk of frames
 *   0x08: samples to discard in the beginning of this block (MPEG/XMA2/Vorbis only?)
 *   0x0c: samples in channel (for MPEG/XMA2 can vary between channels)
 *         full samples without removing samples to discard
 *   (next fields don't exist in later versions for IMA or AT9)
 *   0x10: (MPEG only, empty otherwise) close to number of frames but varies a bit?
 *   0x14: (MPEG only, empty otherwise) channel chunk size (not counting padding)
 * - for each channel (seek table)
 *   32b * entries = global samples per frame in each block (for MPEG probably per full frame)
 *   (AT9 doesn't have a seek table as it's CBR)
 * - per channel (ATRAC9/DSP extra info):
 *   0x00: "D11A"
 *   0x04: frame size
 *   0x06: frame samples
 *   0x08: flags? (0x0103=AT9, 0x0104=DSP)
 *   0x0a: sample rate
 *   0x0c: ATRAC9 config (repeated but same for all blocks) or "D11E" (DSP)
 *   0x10-0x70: padding with 0x77 (ATRAC3) or standard DSP header for original full file (DSP)
 * - padding depending on codec (AT9/DSP: none, MPEG/XMA: closest 0x800)
 */
static bool read_awb_block(STREAMFILE* sf, awc_header* awc, awc_block_info_t* bi, uint32_t block_offset) {
    uint32_t channel_entry_size, seek_entry_size, extra_entry_size, header_padding;
    uint32_t offset = block_offset;
    /* read stupid block crap + derived info at once so hopefully it's a bit easier to understand */

    switch(awc->codec) {
        case 0x08: /* Vorbis */
            channel_entry_size = 0x18;
            seek_entry_size = 0x04;
            extra_entry_size = 0x00;
            header_padding = 0x800;
            break;
        case 0x0F: /* ATRAC9 */
            channel_entry_size = 0x10;
            seek_entry_size = 0x00;
            extra_entry_size = 0x70;
            header_padding = 0x00;
            break;
        default:
            goto fail;
    }

    /* channel info table */
    for (int ch = 0; ch < awc->channels; ch++) {
        bi->blk[ch].start_entry        = read_u32le(offset + 0x00, sf);
        bi->blk[ch].entries            = read_u32le(offset + 0x04, sf);
        bi->blk[ch].channel_skip       = read_u32le(offset + 0x08, sf);
        bi->blk[ch].channel_samples    = read_u32le(offset + 0x0c, sf);
        /* others: optional */

        offset += channel_entry_size;
    }

    /* seek table */
    for (int ch = 0; ch < awc->channels; ch++) {
        offset += bi->blk[ch].entries * seek_entry_size;
    }

    /* extra table and derived info */
    for (int ch = 0; ch < awc->channels; ch++) {
        switch(awc->codec) {
            case 0x08: /* Vorbis */
                /* each "frame" here is actually N vorbis frames then padding up to 0x800 (more or less like a big Ogg page) */
                bi->blk[ch].chunk_size = bi->blk[ch].entries * 0x800;
                break;
            
            case 0x0F: { /* ATRAC9 */
                uint16_t frame_size = read_u16le(offset + 0x04, sf);
    
                bi->blk[ch].chunk_size = bi->blk[ch].entries * frame_size;
                bi->blk[ch].extradata = read_u32be(offset + 0x0c, sf);
                break;
            }
            default:
                goto fail;
        }
        offset += extra_entry_size;
    }

    /* header done, move into data start */
    if (header_padding) {
        /* padding on the current size rather than file offset (block meant to be read into memory, probably) */
        uint32_t header_size = offset - block_offset;
        offset = block_offset + align_size_to_block(header_size, header_padding);
    }

    /* set frame starts per channel */
    for (int ch = 0; ch < awc->channels; ch++) {
        bi->blk[ch].chunk_start = offset;
        offset += bi->blk[ch].chunk_size;
    }

    /* beyond this is padding until awc.block_chunk */

    return true;
fail:
    return false;
}

/* ************************************************************************* */

static VGMSTREAM* build_block_vgmstream(STREAMFILE* sf, awc_header* awc, int channel, awc_block_info_t* bi) {
    VGMSTREAM* vgmstream = NULL;
    awc_block_t* blk = &bi->blk[channel];
    int block_channels = 1;

    //;VGM_LOG("AWC: build ch%i at o=%x, s=%x\n", channel, blk->chunk_start, blk->chunk_size);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(block_channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = awc->sample_rate;
    vgmstream->num_samples = blk->channel_samples - blk->channel_skip;
    vgmstream->stream_size = blk->chunk_size;
    vgmstream->meta_type = meta_AWC;

    switch(awc->codec) {
#ifdef VGM_USE_VORBIS
        case 0x08: {
            vorbis_custom_config cfg = {0};

            cfg.channels = 1;
            cfg.sample_rate = awc->sample_rate;
            cfg.header_offset = awc->vorbis_offset[channel]; /* setup page goes first */
            //cfg.skip_samples = skip_samples; //todo
        
            vgmstream->codec_data = init_vorbis_custom(sf, blk->chunk_start, VORBIS_AWC, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case 0x0F: {
            atrac9_config cfg = {0};

            cfg.channels = block_channels;
            cfg.encoder_delay = blk->channel_skip;
            cfg.config_data = blk->extradata;
            ;VGM_ASSERT(blk->channel_skip, "AWC discard found\n");

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

    if (!vgmstream_open_stream(vgmstream, sf, blk->chunk_start))
        goto fail;
    return vgmstream;
fail:
    ;VGM_LOG("AWB: can't open decoder\n");
    close_vgmstream(vgmstream);
    return NULL;
}

/* per channel to possibly simplify block entry skips, though can't be handled right now */
static VGMSTREAM* build_blocks_vgmstream(STREAMFILE* sf, awc_header* awc, int channel) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;
    int blocks = awc->block_count;
    awc_block_info_t bi = {0};


    /* init layout */
    data = init_layout_segmented(blocks);
    if (!data) goto fail;


    /* one segment per block of this channel */
    for (int i = 0; i < blocks; i++) {
        uint32_t block_offset = awc->stream_offset + awc->block_chunk * i;

        if (!read_awb_block(sf, awc, &bi, block_offset))
            goto fail;

        //;VGM_LOG("AWC: block=%i at %x\n", i, block_offset);
        data->segments[i] = build_block_vgmstream(sf, awc, channel, &bi);
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

/* Make layers per channel for AWC's abhorrent blocks (see read_awb_block).
 *
 * A "music" .awc has N channels = N streams (each using their own mono decoder) chunked in "blocks".
 * Each block then has header + seek table + etc for all channels. But when blocks change, each channel
 * may have a "skip samples" value and blocks repeat some data from last block, so output PCM must be
 * discarded to avoid channels desyncing. Channels in a block don't need to have the same number of samples.
 * (mainly seen in MPEG).
 */
//TODO: this method won't fully work, needs feed decoder + block handler that interacts with decoder(s?)
// (doesn't use multiple decoders since default encoder delay in Vorbis would discard too much per block)
//
// When blocks change presumably block handler needs to tell decoder to finish decoding all from prev block
// then skip samples from next decodes. Also since samples may vary per channel, each would handle blocks
// independently.
//
// This can be simulated by making one decoder per block (segmented, but opens too many SFs and can't skip
// samples correctly), or with a custom STREAMFILE that skips repeated block (works ok-ish but not all codecs).
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

