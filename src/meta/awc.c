#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"
#include "awc_streamfile.h"
#include "awc_decryption_streamfile.h"

typedef struct {
    bool big_endian;
    bool is_encrypted;
    bool is_streamed; /* implicit: streams=music, sfx=memory */
    bool is_alt;

    int total_subsongs;

    int channels;
    int sample_rate;
    int num_samples;
    uint8_t codec;

    int block_count;
    int block_chunk;

    uint32_t tags_offset;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t vorbis_offset[AWC_MAX_MUSIC_CHANNELS];

    /* stream+music only */
    uint32_t channel_hash[AWC_MAX_MUSIC_CHANNELS];
    struct {
        uint32_t hash_id;
        int tag_count;
    } stream_info[AWC_MAX_MUSIC_CHANNELS];
} awc_header;

static int parse_awc_header(STREAMFILE* sf, awc_header* awc);

static layered_layout_data* build_layered_awc(STREAMFILE* sf, awc_header* awc);


/* AWC - Audio Wave Container from RAGE (Rockstar Advanced Game Engine) [GTA5 (multi), Red Dead Redemption (multi), Max Payne 3 (multi)] */
VGMSTREAM* init_vgmstream_awc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    awc_header awc = {0};


    /* checks */
    if (!parse_awc_header(sf, &awc))
        return NULL;
    if (!check_extensions(sf,"awc"))
        return NULL;

    if (awc.is_encrypted) {
        /* seen in GTA5 PC/PS4, music or sfx (not all files) */
        sf_body = setup_awcd_streamfile(sf, awc.stream_offset, awc.stream_size, awc.block_chunk);
        if (!sf_body) {
            vgm_logi("AWC: encrypted data found, needs .awckey\n");
            goto fail;
        }
    }
    else {
        sf_body = sf;
    }


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
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf_body, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_FFmpeg;
            }
            else {
                /* regular XMA for sfx */
                vgmstream->codec_data = init_ffmpeg_xma2_raw(sf_body, awc.stream_offset, awc.stream_size, awc.num_samples, awc.channels, awc.sample_rate, 0, 0);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;

                xma_fix_raw_samples(vgmstream, sf_body, awc.stream_offset,awc.stream_size, 0, 0,0); /* samples are ok? */
            }

            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x07: {    /* MPEG (PS3) */
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf_body, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_MPEG_custom;
            }
            else {
                vgmstream->codec_data = init_mpeg_custom(sf_body, awc.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, NULL);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }
            break;
        }
#endif

#ifdef VGM_USE_VORBIS
        case 0x08: {    /* Vorbis (PC) [Red Dead Redemption 2 (PC)] */
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf_body, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_VORBIS_custom;
            }
            else {
                vorbis_custom_config cfg = {0};

                cfg.channels = awc.channels;
                cfg.sample_rate = awc.sample_rate;
                cfg.header_offset = awc.vorbis_offset[0];

                vgmstream->codec_data = init_vorbis_custom(sf_body, awc.stream_offset, VORBIS_AWC, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
                vgmstream->coding_type = coding_VORBIS_custom;
            }
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x0F: {    /* ATRAC9 (PS4) [Red Dead Redemption (PS4)] */
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf_body, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_ATRAC9;
            }
            else {
                VGMSTREAM* temp_vs = NULL;
                STREAMFILE* temp_sf = NULL;

                temp_sf = setup_subfile_streamfile(sf_body, awc.stream_offset, awc.stream_size, "at9");
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
                dsp_read_coefs_le(vgmstream, sf_body, awc.stream_offset + 0x1c + 0x00, 0x00);
                dsp_read_hist_le (vgmstream, sf_body, awc.stream_offset + 0x1c + 0x20, 0x00);
                awc.stream_offset += 0x60;

                /* shouldn't be possible since it's only used for sfx anyway */
                if (awc.channels > 1)
                    goto fail;
            } 
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0D: {    /* OPUS (PC) [Red Dead Redemption (PC)] */
            if (awc.is_streamed) {
                vgmstream->layout_data = build_layered_awc(sf_body, &awc);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_FFmpeg;
            }
            else {
                VGM_LOG("AWC: unknown non-streamed Opus mode\n");
                goto fail;
            }
            break;
        }
#endif

        case 0x11: {    /* RIFF-MSADPCM (PC) [Red Dead Redemption (PC)] */
            if (awc.is_streamed) {
                VGM_LOG("AWC: unknown streamed mode for codec 0x%02x\n", awc.codec);
                goto fail;
            }
            else {
                VGMSTREAM* temp_vs = NULL;
                STREAMFILE* temp_sf = NULL;

                temp_sf = setup_subfile_streamfile(sf_body, awc.stream_offset, awc.stream_size, "wav");
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

        case 0xFF:
            vgmstream->coding_type = coding_SILENCE;
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "[%s]", "midi");
            break;

        default:
            VGM_LOG("AWC: unknown codec 0x%02x\n", awc.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf_body, awc.stream_offset))
        goto fail;
    if (sf_body != sf) close_streamfile(sf_body);
    return vgmstream;

fail:
    if (sf_body != sf) close_streamfile(sf_body);
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
    read_u64_t read_u64 = NULL;
    read_u32_t read_u32 = NULL;
    read_u16_t read_u16 = NULL;
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
        return false;
    }

    /* stream tag starts (ex. stream#0 = 0, stream#1 = 4, stream#2 = 7: to read tags from stream#2 skip to 7th tag) */
    if (flags & 0x00010000)
        offset += 0x2 * entries;

    /* seems to indicate chunks are not ordered (ie. header structures from tags may go after data), usually for non-streams */
    //if (flags & 0x00020000)
    //  awc->is_unordered = true;

    /* stream/multichannel flag? (GTA5, some RDR2), can be used to detect some odd behavior in GTA5 vs RDR1 */
    if (flags & 0x00040000)
        awc->is_alt = true;

    /* encrypted data chunk (most of GTA5 PC for licensed audio) */
    if (flags & 0x00080000)
        awc->is_encrypted = true;

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
        /* array access below */
        if (entries > AWC_MAX_MUSIC_CHANNELS)
            goto fail;
    }
    else { /* sfx pack, each stream is a sound */
        awc->total_subsongs = entries;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > awc->total_subsongs || awc->total_subsongs < 1) goto fail;
    }


    /** stream ids and tag counts **/
    for (int i = 0; i < entries; i++) {
        uint32_t info_header = read_u32(offset + 0x00, sf);
        int entry_count  = (info_header >> 29) & 0x7; /* 3b */
        uint32_t hash_id = (info_header >>  0) & 0x1FFFFFFF; /* 29b */
        
        if (i + 1 < target_subsong)
            tags_skip += entry_count; /* tags to skip to reach target's tags, in the next header */
        if (target_subsong == i + 1)
            tag_count = entry_count;

        if (awc->is_streamed) {
            awc->stream_info[i].hash_id = hash_id;
            awc->stream_info[i].tag_count = entry_count;
        }

        offset += 0x04;
    }
    awc->tags_offset = offset; /* where tags for stream start */

    offset += 0x08 * tags_skip; /* ignore tags for other streams */



    /** tags per stream **/
    for (int i = 0; i < tag_count; i++) {
        uint64_t tag_header = read_u64(offset + 0x08*i,sf);
        uint8_t  tag_type   = ((tag_header >> 56) & 0xFF); /* 8b */
        uint32_t tag_size   = ((tag_header >> 28) & 0x0FFFFFFF); /* 28b */
        uint32_t tag_offset = ((tag_header >>  0) & 0x0FFFFFFF); /* 28b */

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
                    /* extremely rare but doesn't seem to matter, some streams are dummies (RDR2 STREAMS/ABIGAIL_HUMMING_*) */ 
                    //goto fail;
                }

                for (int ch = 0; ch < awc->channels; ch++) {
                    int num_samples, sample_rate, codec;

                    awc->channel_hash[ch] = read_u32(tag_offset + 0x0c + 0x10*ch + 0x00,sf);  /* reference, for vorbis */
                    num_samples = read_u32(tag_offset + 0x0c + 0x10*ch + 0x04,sf);
                    /* 0x08: headroom */
                    sample_rate = read_u16(tag_offset + 0x0c + 0x10*ch + 0x0a,sf);
                    codec = read_u8(tag_offset + 0x0c + 0x10*ch + 0x0c,sf);
                    /* 0x0d(8): round size? */
                    /* 0x0e: unknown (zero/-1, loop flag? BOB_FINALE_1_A.awc, but also set in stingers) */

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
                /* 0x04: -1? (loop related?) */
                awc->sample_rate = read_u16(tag_offset + 0x08,sf);
                /* 0x0a: headroom */
                /* 0x0c: unknown (loop related?) */
                /* 0x0e: unknown (loop related?) */
                /* 0x10: unknown (loop related?) */
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
                if (read_u16(tag_offset + 0x1e,sf))/* rarely not set and uses a tag below */
                    awc->vorbis_offset[0] = tag_offset + 0x20; /* data up to vorbis setup size */

                awc->channels = 1;
                break;

            case 0x7F: /* vorbis setup */
                if (awc->is_streamed) {
                    /* music stream doesn't have this (instead every channel-strem have one, parsed later) */
                    VGM_LOG("AWC: vorbis setup found but streamed\n");
                    goto fail;
                }

                /* very rarely used for sfx: SS_AM/GESTURE01.awc */
                awc->vorbis_offset[0] = tag_offset;
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
            default:   /* 0x68=midi?, 0x5A/0xD9=? */
                //VGM_LOG("AWC: ignoring unknown tag 0x%02x\n", tag);
                break;
        }
    }

    /* in music mode there tags for other streams we don't use, except for vorbis. streams have vorbis setup info for channels, but
     * channel<>stream order doesn't match, so we need to assign setup to channels. All setups seem to be the same though. */
    if (awc->is_streamed && awc->codec == 0x08) {
        offset = awc->tags_offset;
        offset += 0x08 * awc->stream_info[0].tag_count; /* ignore 1st/music stream */

        for (int stream = 1; stream < entries; stream++) {
            for (int tag = 0; tag < awc->stream_info[stream].tag_count; tag++) {
                uint64_t tag_header = read_u64(offset,sf);
                uint8_t  tag_type   = ((tag_header >> 56) & 0xFF); /* 8b */
              //uint32_t tag_size   = ((tag_header >> 28) & 0x0FFFFFFF); /* 28b */
                uint32_t tag_offset = ((tag_header >>  0) & 0x0FFFFFFF); /* 28b */

                switch(tag_type) {
                    case 0x7f: /* vorbis setup */
                        /* find which channel uses this stream's data */
                        for (int ch = 0; ch < awc->channels; ch++) {
                            if (awc->channel_hash[ch] == awc->stream_info[stream].hash_id) {
                                awc->vorbis_offset[ch] = tag_offset;
                                //awc->vorbis_size[ch] = tag_size; /* not needed (implicit)*/
                                break;
                            }
                        }
                        break;
                    default:
                        break;
                }

                offset += 0x08;
            }
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

static VGMSTREAM* build_blocks_vgmstream(STREAMFILE* sf, awc_header* awc, int channel) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int block_channels = 1;
    uint32_t substream_size, substream_offset;


    /* setup custom IO streamfile that removes AWC's odd blocks (not perfect but serviceable) */
    temp_sf = setup_awc_streamfile(sf, awc->stream_offset, awc->stream_size, awc->block_chunk, awc->channels, channel, awc->codec, awc->big_endian, awc->is_alt);
    if (!temp_sf) goto fail;

    substream_offset = 0x00;
    substream_size = get_streamfile_size(temp_sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(block_channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AWC;
    vgmstream->sample_rate = awc->sample_rate;
    vgmstream->num_samples = awc->num_samples;
    vgmstream->stream_size = awc->stream_size;

    vgmstream->stream_size = substream_size;


    switch(awc->codec) {
#ifdef VGM_USE_FFMPEG
        case 0x05: {    /* XMA2 (X360) */
            vgmstream->codec_data = init_ffmpeg_xma2_raw(temp_sf, substream_offset, substream_size, awc->num_samples, block_channels, awc->sample_rate, 0, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, temp_sf, substream_offset, substream_size, 0, 0,0); /* samples are ok? */
            break;
        }
#endif
#ifdef VGM_USE_MPEG
        case 0x07: { /* MPEG (PS3) */
            vgmstream->codec_data = init_mpeg_custom(temp_sf, substream_offset, &vgmstream->coding_type, block_channels, MPEG_STANDARD, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case 0x08: {
            vorbis_custom_config cfg = {0};

            cfg.channels = 1;
            cfg.sample_rate = awc->sample_rate;
            cfg.header_offset = awc->vorbis_offset[channel]; /* setup page goes separate */

            /* note that it needs sf on init to read the header + start offset for later, and temp_sf on decode to read data */
            vgmstream->codec_data = init_vorbis_custom(sf, substream_offset, VORBIS_AWC, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 0x0D: {
            opus_config cfg = {0};

            /* read from first block (all blocks have it but same thing), see awc_streamfile.h */
            uint32_t frame_size_offset = awc->stream_offset + 0x10 * awc->channels + 0x70 * channel + 0x04;

            cfg.frame_size = read_u16le(frame_size_offset, sf); // always 0x50?
            cfg.channels = 1;

            vgmstream->codec_data = init_ffmpeg_fixed_opus(temp_sf, substream_offset, substream_size, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case 0x0F: {
            atrac9_config cfg = {0};

            /* read from first block (all blocks have it but same thing), see awc_streamfile.h */
            uint32_t extradata_offset = awc->stream_offset + 0x10 * awc->channels + 0x70 * channel + 0x0c;

            cfg.channels = block_channels;
            cfg.encoder_delay = 0; //?
            cfg.config_data = read_u32be(extradata_offset, sf);

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

    if (!vgmstream_open_stream(vgmstream, temp_sf, substream_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;
fail:
    ;VGM_LOG("AWB: can't open decoder\n");
    close_vgmstream(vgmstream);
    close_streamfile(temp_sf);
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
 * 
 * For most repeated data seems to be exact copies of prev block, so data can be skipped (rather than samples) and still get
 * proper non-desynced audio. This data is probably needed to reset decoders between seekable blocks.
 */
static layered_layout_data* build_layered_awc(STREAMFILE* sf, awc_header* awc) {
    layered_layout_data* data = NULL;


    /* init layout */
    data = init_layout_layered(awc->channels);
    if (!data) goto fail;

    /* open each layer subfile */
    for (int i = 0; i < awc->channels; i++) {
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
