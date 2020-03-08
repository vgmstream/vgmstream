#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "awc_xma_streamfile.h"

typedef struct {
    int big_endian;
    int is_encrypted;
    int is_music;

    int total_subsongs;

    int channel_count;
    int sample_rate;
    int codec;
    int num_samples;

    int block_chunk;

    off_t stream_offset;
    size_t stream_size;

} awc_header;

static int parse_awc_header(STREAMFILE* streamFile, awc_header* awc);


/* AWC - from RAGE (Rockstar Advanced Game Engine) audio [Red Dead Redemption, Max Payne 3, GTA5 (multi)] */
VGMSTREAM * init_vgmstream_awc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    awc_header awc = {0};


    /* checks */
    if (!check_extensions(streamFile,"awc"))
        goto fail;
    if (!parse_awc_header(streamFile, &awc))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(awc.channel_count, 0);
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
                data = init_layout_layered(awc.channel_count);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->layout_type = layout_layered;
                vgmstream->coding_type = coding_FFmpeg;

                /* open each layer subfile */
                for (i = 0; i < awc.channel_count; i++) {
                    STREAMFILE* temp_streamFile;
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
                    temp_streamFile = setup_awc_xma_streamfile(streamFile, awc.stream_offset, awc.stream_size, awc.block_chunk, awc.channel_count, i);
                    if (!temp_streamFile) goto fail;

                    substream_offset = 0; /* where FFmpeg thinks data starts, which our custom streamFile will clamp */
                    substream_size = get_streamfile_size(temp_streamFile); /* data of one XMA substream without blocks */
                    block_size = 0x8000; /* no idea */
                    block_count = substream_size / block_size; /* not accurate but not needed */

                    bytes = ffmpeg_make_riff_xma2(buf, 0x100, awc.num_samples, substream_size, layer_channels, awc.sample_rate, block_count, block_size);
                    data->layers[i]->codec_data = init_ffmpeg_header_offset(temp_streamFile, buf,bytes, substream_offset,substream_size);

                    xma_fix_raw_samples(data->layers[i], temp_streamFile, substream_offset,substream_size, 0, 0,0); /* samples are ok? */

                    close_streamfile(temp_streamFile);
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

                bytes = ffmpeg_make_riff_xma2(buf, 0x100, awc.num_samples, awc.stream_size, awc.channel_count, awc.sample_rate, block_count, block_size);
                vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, awc.stream_offset,awc.stream_size);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;

                xma_fix_raw_samples(vgmstream, streamFile, awc.stream_offset,awc.stream_size, 0, 0,0); /* samples are ok? */
            }

            break;
        }


#endif
#ifdef VGM_USE_MPEG
        case 0x07: {    /* MPEG (PS3) */
            mpeg_custom_config cfg = {0};

            cfg.chunk_size = awc.block_chunk;
            cfg.big_endian = awc.big_endian;

            vgmstream->codec_data = init_mpeg_custom(streamFile, awc.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_AWC, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        default:
            VGM_LOG("AWC: unknown codec 0x%02x\n", awc.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,awc.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* Parse Rockstar's AWC header (much info from LibertyV: https://github.com/koolkdev/libertyv).
 * Made of entries for N streams, each with a number of tags pointing to chunks (header, data, events, etc). */
static int parse_awc_header(STREAMFILE* streamFile, awc_header* awc) {
    int64_t (*read_64bit)(off_t,STREAMFILE*) = NULL;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int i, ch, entries;
    uint32_t flags, info_header, tag_count = 0, tags_skip = 0;
    off_t off;
    int target_subsong = streamFile->stream_index;


    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41444154 &&  /* "ADAT" (LE) */
        read_32bitBE(0x00,streamFile) != 0x54414441)    /* "TADA" (BE) */
        goto fail;

    awc->big_endian = read_32bitBE(0x00,streamFile) == 0x54414441;
    if (awc->big_endian) {
        read_64bit = read_64bitBE;
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_64bit = read_64bitLE;
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }


    flags = read_32bit(0x04,streamFile);
    entries = read_32bit(0x08,streamFile);
    //header_size = read_32bit(0x0c,streamFile); /* after to stream id/tags, not including chunks */

    off = 0x10;

    if ((flags & 0xFF00FFFF) != 0xFF000001 || (flags & 0x00F00000)) {
        VGM_LOG("AWC: unknown flags 0x%08x\n", flags);
        goto fail;
    }

    if (flags & 0x00010000) /* some kind of mini offset table */
        off += 0x2 * entries;
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
    awc->is_music = (read_32bit(off + 0x00,streamFile) & 0x1FFFFFFF) == 0x00000000;
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
        info_header = read_32bit(off + 0x04*i, streamFile);
        tag_count   = (info_header >> 29) & 0x7; /* 3b */
        //id        = (info_header >>  0) & 0x1FFFFFFF; /* 29b */
        if (target_subsong-1 == i)
            break;
        tags_skip += tag_count; /* tags to skip to reach target's tags, in the next header */
    }
    off += 0x04*entries;
    off += 0x08*tags_skip;

    /* get stream tags */
    for (i = 0; i < tag_count; i++) {
        uint64_t tag_header;
        uint8_t tag;
        size_t size;
        off_t offset;

        tag_header = (uint64_t)read_64bit(off + 0x08*i,streamFile);
        tag    = (uint8_t)((tag_header >> 56) & 0xFF); /* 8b */
        size   =  (size_t)((tag_header >> 28) & 0x0FFFFFFF); /* 28b */
        offset =   (off_t)((tag_header >>  0) & 0x0FFFFFFF); /* 28b */

        /* Tags are apparently part of a hash derived from a word ("data", "format", etc).
         * If music + 1ch, the header and data chunks can repeat for no reason (sometimes not even pointed). */
        switch(tag) {
            case 0x55: /* data */
                awc->stream_offset = offset;
                awc->stream_size = size;
                break;

            case 0x48: /* music header */
                if (!awc->is_music) {
                    VGM_LOG("AWC: music header found in sfx\n");
                    goto fail;
                }

                /* 0x00(32): unknown (some count?) */
                awc->block_chunk = read_32bit(offset + 0x04,streamFile);
                awc->channel_count = read_32bit(offset + 0x08,streamFile);

                if (awc->channel_count != entries - 1) { /* not counting id-0 */
                    VGM_LOG("AWC: number of music channels doesn't match entries\n");
                    goto fail;
                }

                for (ch = 0; ch < awc->channel_count; ch++) {
                    int num_samples, sample_rate, codec;
                    /* 0x00(32): stream id (not always in the header entries order) */
                    /* 0x08(16): headroom?, 0x0d(8): round size?, 0x0e(16): unknown (zero?) */
                    num_samples = read_32bit(offset + 0x0c + 0x10*ch + 0x04,streamFile);
                    sample_rate = (uint16_t)read_16bit(offset + 0x0c + 0x10*ch + 0x0a,streamFile);
                    codec = read_8bit(offset + 0x0c + 0x10*ch + 0x0c, streamFile);

                    /* validate channels differences */
                    if ((awc->num_samples && !(awc->num_samples >= num_samples - 10 && awc->num_samples <= num_samples + 10)) ||
                        (awc->sample_rate && awc->sample_rate != sample_rate)) {
                        VGM_LOG("AWC: found header diffs in channel %i, ns=%i vs %i, sr=%i vs %i\n",
                                ch, awc->num_samples, num_samples, awc->sample_rate, sample_rate);
                        /* sometimes (often cutscenes in Max Payne 3 and RDR DLC) channels have bif sample diffs,
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
                /*  0x04(32): -1?, 0x0a(16x4): unknown x4, 0x12: null? */
                awc->num_samples = read_32bit(offset + 0x00,streamFile);
                awc->sample_rate = (uint16_t)read_16bit(offset + 0x08,streamFile);
                awc->codec = read_8bit(offset + 0x13, streamFile);
                awc->channel_count = 1;
                break;

            case 0xA3: /* block-to-sample table (32b x number of blocks w/ num_samples at the start of each block) */
            case 0xBD: /* events (32bx4): type_hash, params_hash, timestamp_ms, flags */
            default: /* 0x5C=animation/RSC?,  0x68=midi?, 0x36/0x2B/0x5A/0xD9=? */
                //VGM_LOG("AWC: ignoring unknown tag 0x%02x\n", tag);
                break;
        }
    }

    if (!awc->stream_offset) {
        VGM_LOG("AWC: stream offset not found\n");
        goto fail;
    }

    /* If music, data is divided into blocks of block_chunk size with padding.
     * Each block has a header/seek table and interleaved data for all channels */
    if (awc->is_music && read_32bit(awc->stream_offset, streamFile) != 0) {
        VGM_LOG("AWC: music found, but block doesn't start with seek table at %x\n", (uint32_t)awc->stream_offset);
        goto fail;
    }


    return 1;
fail:
    return 0;
}
