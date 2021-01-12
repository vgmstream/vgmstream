#include "meta.h"
#include "../coding/coding.h"

typedef struct {
    int total_subsongs;
    int codec;
    int channels;
    int sample_rate;

    int block_count;
    int block_size;

    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;

    off_t stream_offset;
    off_t stream_size;
    off_t extra_offset;
    
    off_t name_offset;
} aac_header;

static int parse_aac(STREAMFILE* sf, aac_header* aac);


/* AAC - tri-Ace (ASKA engine) Audio Container */
VGMSTREAM* init_vgmstream_ta_aac(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    aac_header aac = {0};


    /* checks */
    /* .aac: actual extension, .laac: for players to avoid hijacking MP4/AAC */
    if (!check_extensions(sf, "aac,laac"))
        goto fail;
    if (!is_id32be(0x00, sf, "AAC ") && !is_id32le(0x00, sf, "AAC "))
        goto fail;

    if (!parse_aac(sf, &aac))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(aac.channels, aac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_TA_AAC;
    vgmstream->sample_rate = aac.sample_rate;
    vgmstream->num_streams = aac.total_subsongs;
    vgmstream->stream_size = aac.stream_size;

    switch(aac.codec) {

#ifdef VGM_USE_FFMPEG
        case 0x0165: { /* Infinite Undiscovery (X360), Star Ocean 4 (X360), Resonance of Fate (X360) */
            uint8_t buf[0x100];
            size_t bytes;

            bytes = ffmpeg_make_riff_xma2(buf, sizeof(buf), aac.num_samples, aac.stream_size, aac.channels, aac.sample_rate, aac.block_count, aac.block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf, bytes, aac.stream_offset, aac.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = aac.num_samples;
            vgmstream->loop_start_sample = aac.loop_start;
            vgmstream->loop_end_sample = aac.loop_end;

            xma_fix_raw_samples(vgmstream, sf, aac.stream_offset, aac.stream_size, 0, 0,1);
            break;
        }

        case 0x04: 
        case 0x05: 
        case 0x06: { /* Resonance of Fate (PS3), Star Ocean 4 (PS3) */
            int block_align = (aac.codec == 0x04 ? 0x60 : (aac.codec == 0x05 ? 0x98 : 0xC0)) * aac.channels;
            int encoder_delay = 1024 + 69; /* AT3 default, gets good loops */

            vgmstream->num_samples = atrac3_bytes_to_samples(aac.stream_size, block_align) - encoder_delay;
            /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
            vgmstream->loop_start_sample = atrac3_bytes_to_samples(aac.loop_start, block_align); // - encoder_delay
            vgmstream->loop_end_sample = atrac3_bytes_to_samples(aac.loop_end, block_align) - encoder_delay;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, aac.stream_offset, aac.stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case 0x08:   /* Judas Code (Vita) */
        case 0x18: { /* Resonance of Fate (PS4) */
            atrac9_config cfg = {0};
            cfg.channels = vgmstream->channels;

            if (aac.codec == 0x08) {
                /* 0x00: ? (related to bitrate/channels?) */
                cfg.encoder_delay = read_s32le(aac.extra_offset + 0x04,sf);
                cfg.config_data   = read_u32be(aac.extra_offset + 0x08,sf);
            }
            else {
                /* 0x00: ? (related to bitrate/channels?) */
                cfg.encoder_delay = read_s16le(aac.extra_offset + 0x04,sf);
                /* 0x06: samples per frame */
                /* 0x08: num samples (without encoder delay) */
                cfg.config_data   = read_u32le(aac.extra_offset + 0x0c,sf);
                /* 0x10: loop start sample (without encoder delay) */
                /* 0x14: loop end sample (without encoder delay) */
                /* 0x18: related to loop start (adjustment? same as loop start when less than a sample) */
                /* using loop samples causes clicks in some tracks, so maybe it's info only,
                 * or it's meant to be adjusted with value at 0x18 */
            }

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = atrac9_bytes_to_samples(aac.stream_size, vgmstream->codec_data);
            vgmstream->num_samples -= cfg.encoder_delay;
            vgmstream->loop_start_sample = atrac9_bytes_to_samples(aac.loop_start, vgmstream->codec_data);
            vgmstream->loop_end_sample = atrac9_bytes_to_samples(aac.loop_end, vgmstream->codec_data);
            break;
        }
#endif
        case 0x0a: /* Star Ocean 4 (PC) */
            if (aac.channels > 2) goto fail; /* unknown data layout */
            /* 0x00: some value * channels? */
            /* 0x04: frame size */
            /* 0x08: frame samples */

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_u32le(aac.extra_offset + 0x04,sf);

            vgmstream->num_samples = msadpcm_bytes_to_samples(aac.stream_size, vgmstream->frame_size, aac.channels);
            vgmstream->loop_start_sample = msadpcm_bytes_to_samples(aac.loop_start, vgmstream->frame_size, aac.channels);
            vgmstream->loop_end_sample = msadpcm_bytes_to_samples(aac.loop_end, vgmstream->frame_size, aac.channels);
            break;

        case 0x0d: /* Star Ocean Anamnesis (Android), Heaven x Inferno (iOS), Star Ocean 4 (PC), Resonance of Fate (PC) */
            /* 0x00: 0x17700 * channels? */
            /* 0x04: frame size */
            /* 0x08: frame samples (not always?) */

            vgmstream->coding_type = coding_ASKA;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_u32le(aac.extra_offset + 0x04,sf); /* usually 0x40, rarely 0x20/C0 (ex. some ROF PC) */
            /* N-channel frames are allowed (ex. 4/6ch in SO4/ROF PC) */
            if (vgmstream->frame_size > 0xc0) /* known max */
                goto fail;

            vgmstream->num_samples = aska_bytes_to_samples(aac.stream_size, vgmstream->frame_size, aac.channels);
            vgmstream->loop_start_sample = aska_bytes_to_samples(aac.loop_start, vgmstream->frame_size, aac.channels);
            vgmstream->loop_end_sample = aska_bytes_to_samples(aac.loop_end, vgmstream->frame_size, aac.channels);
            break;

#ifdef VGM_USE_VORBIS
        case 0x0e: { /* Star Ocean Anamnesis (Android-v1.9.2), Heaven x Inferno (iOS) */
            vgmstream->codec_data = init_ogg_vorbis(sf, aac.stream_offset, aac.stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = aac.num_samples;
            vgmstream->loop_start_sample = read_s32le(aac.extra_offset + 0x00,sf);
            vgmstream->loop_end_sample = read_s32le(aac.extra_offset + 0x04,sf);
            /* seek table after loops */
            break;
        }
#endif
        default:
            VGM_LOG("AAC: unknown codec %x\n", aac.codec);
            goto fail;
    }

    if (aac.name_offset)
        read_string(vgmstream->stream_name, STREAM_NAME_SIZE, aac.name_offset, sf);

    if (!vgmstream_open_stream(vgmstream, sf, aac.stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* DIR/dirn + WAVE chunk [Infinite Undiscovery (X360), Star Ocean 4 (X360)] */
static int parse_aac_v1(STREAMFILE* sf, aac_header* aac) {
    off_t offset, test_offset, wave_offset;
    int target_subsong = sf->stream_index;

    /* base header */
    /* 0x00: id */
    /* 0x04: size */
    /* 0x10: subsongs */
    /* 0x14: base size */
    /* 0x14: head size */
    /* 0x18: data size */
    /* 0x20: config? (0x00010003) */
    /* 0x30+ DIR + dirn subsongs */

    if (!is_id32be(0x30, sf, "DIR "))
        goto fail;
    aac->total_subsongs = read_u32be(0x40, sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > aac->total_subsongs || aac->total_subsongs < 1) goto fail;

    {
        int i;
        offset = 0;
        test_offset = 0x50;
        for (i = 0; i < aac->total_subsongs; i++) {
            uint32_t entry_type = read_u32be(test_offset + 0x00, sf);
            uint32_t entry_size = read_u32be(test_offset + 0x04, sf);

            switch(entry_type) {
                case 0x6469726E: /* "dirn" */
                    if (i + 1 == target_subsong) {
                        aac->name_offset = test_offset + 0x10;
                        offset = read_u32be(test_offset + 0x90, sf); /* absolute */
                    }
                    break;

                default:
                    goto fail;
            }

            test_offset += entry_size;
        }
    }

    if (!is_id32be(offset + 0x00, sf, "WAVE"))
        goto fail;
    wave_offset = offset;
    offset += 0x10;

    {
        /* X360 */
        int i, streams;
        off_t strm_offset;
        
        /* 0x00: 0x0400 + song ID */
        streams             = read_u16be(offset + 0x04, sf);
        aac->codec          = read_u16be(offset + 0x06, sf);
        /* 0x08: null */
        /* 0x0c: null */
        aac->stream_size    = read_u32be(offset + 0x10, sf);
        aac->sample_rate    = read_s32be(offset + 0x14, sf);
        aac->loop_start     = read_u32be(offset + 0x18, sf);
        aac->loop_end       = read_u32be(offset + 0x1C, sf); /* max samples if not set */
        aac->block_size     = read_u32be(offset + 0x20, sf);
        /* 0x24: max samples */
        aac->num_samples    = read_u32be(offset + 0x28, sf);
        aac->block_count    = read_u32be(offset + 0x2c, sf);
        
        /* one UI file has a smaller header, early version? */
        if (read_u32be(offset + 0x30, sf) == 0x7374726D) {
            aac->loop_flag      = 0; /* ? */
            strm_offset = 0x30;
        }
        else {
            /* 0x30: null */
            /* 0x34: encoder delay? */
            aac->loop_flag      = read_u32be(offset + 0x38, sf) != 0; /* loop end block */
            /* 0x3c: size? (loop-related) */
            strm_offset = 0x40;
        }

        aac->stream_offset  = wave_offset + 0x1000;
        
        /* channels depends on streams definitions, "strm" chunk (max 2ch per strm) */
        aac->channels = 0;
        for (i = 0; i < streams; i++) {
            /* format: "strm", size, null, null, channels, ?, sample rate, encoder delay, samples, nulls  */
            aac->channels += read_s8(offset + strm_offset + i*0x30 + 0x10, sf);
        }
    }

    return 1;
fail:
    return 0;
}

/* ASC + WAVE chunks [Resonance of Fate (X360/PS3), Star Ocean 4 (PS3)] */
static int parse_aac_v2(STREAMFILE* sf, aac_header* aac) {
    off_t offset, start, size, test_offset, asc_offset;
    int target_subsong = sf->stream_index;

    /* base header */
    /* 0x00: id */
    /* 0x04: size */
    /* 0x10: config? (0x00020100/0x00020002/0x00020301/etc) */
    /* 0x14: flag (0x80/01) */
    /* 0x18: align? (PS3=0x30, X360=0xFD0) */
    /* 0x28: platform (PS3=3, X360=2) */
    /* 0x30+ offsets+sizes to ASC or GUIDs */

    start = read_u32be(0x2c, sf);
    
    if (target_subsong == 0) target_subsong = 1;
    aac->total_subsongs = 0;

    if (is_id32be(start + 0x00, sf, "AMF ")) {
        /* GUID subsongs */
        if (!is_id32be(start + 0x10, sf, "head"))
            goto fail;
        size = read_u32be(start + 0x10 + 0x10, sf);

        offset = 0;
        test_offset = start + 0x10;
        while (test_offset < start + size) {
            uint32_t entry_type = read_u32be(test_offset + 0x00, sf);
            uint32_t entry_size = read_u32be(test_offset + 0x04, sf);
            
            if (entry_type == 0)
                break;

            switch(entry_type) {
                case 0x61646472: /* "addr" (GUID + config) */
                    aac->total_subsongs++;
                    if (aac->total_subsongs == target_subsong) {
                        offset = read_u32be(test_offset + 0x2c, sf) + start + size;
                    }
                    break;

                default: /* "head", "buff" */
                    break;
            }
            
            test_offset += entry_size;
        }
    }
    else if (is_id32be(start + 0x00, sf, "ASC ")) {
        /* regular subsongs */
        offset = 0;
        for (test_offset = 0x30; test_offset < start; test_offset += 0x10) {
            uint32_t entry_offset = read_u32be(test_offset + 0x00, sf);
            /* 0x04: entry size */

            if (entry_offset) { /* often 0 */
                aac->total_subsongs++;
                if (aac->total_subsongs == target_subsong) {
                    offset = entry_offset;
                }
            }
        }
    }
    else {
        goto fail;
    }

    if (target_subsong < 0 || target_subsong > aac->total_subsongs || aac->total_subsongs < 1) goto fail;

    if (!is_id32be(offset + 0x00, sf, "ASC "))
        goto fail;
    asc_offset = offset;

    /* ASC section has offsets to "PLBK" chunk (?) and "WAVE" (header), may be followed by "VRC " (?) */
    /* 0x50: PLBK offset */
    offset += read_u32be(offset + 0x54, sf); /* WAVE offset */
    if (!is_id32be(offset + 0x00, sf, "WAVE"))
        goto fail;
    offset += 0x10;

    if (read_u16be(offset + 0x00, sf) == 0x0400) {
        /* X360 */
        int i, streams;

        /* 0x00: 0x0400 + song ID? (0) */
        streams             = read_u16be(offset + 0x04, sf);
        aac->codec          = read_u16be(offset + 0x06, sf);
        /* 0x08: null */
        /* 0x0c: null */
        aac->stream_size    = read_u32be(offset + 0x10, sf);
        aac->sample_rate    = read_s32be(offset + 0x14, sf);
        aac->loop_start     = read_u32be(offset + 0x18, sf);
        aac->loop_end       = read_u32be(offset + 0x1C, sf); /* max samples if not set */
        aac->block_size     = read_u32be(offset + 0x20, sf);
        /* 0x24: max samples */
        aac->num_samples    = read_u32be(offset + 0x28, sf);
        aac->block_count    = read_u32be(offset + 0x2c, sf);
        /* 0x30: null */
        /* 0x34: encoder delay? */
        aac->loop_flag      = read_u32be(offset + 0x38, sf) != 0; /* loop end block */
        /* 0x3c: size? (loop-related) */
        aac->stream_offset  = read_u32be(offset + 0x40, sf) + asc_offset;

        /* channels depends on streams definitions, "strm" chunk (max 2ch per strm) */
        aac->channels = 0;
        for (i = 0; i < streams; i++) {
            /* format: "strm", size, null, null, channels, ?, sample rate, encoder delay, samples, nulls  */
            aac->channels += read_s8(offset + 0x44 + i*0x30 + 0x10, sf);
        }

        /* after streams and aligned to 0x10 is "Seek" table */
    }
    else {
        /* PS3 */
        aac->codec          = read_u32be(offset + 0x00, sf);
        aac->channels       = read_u32be(offset + 0x04, sf);
        aac->stream_size    = read_u32be(offset + 0x08, sf); /* usable size (without padding) */
        aac->sample_rate    = read_s32be(offset + 0x0c, sf);
        /* 0x10: 0x51? */
        aac->loop_start     = read_u32be(offset + 0x14, sf);
        aac->loop_end       = read_u32be(offset + 0x18, sf);
        /* 0x1c: null */

        aac->stream_offset  = offset + 0x20;
    }

    aac->loop_flag = (aac->loop_start != -1);

    return 1;
fail:
    return 0;
}

/* AAOB + WAVE + WAVB chunks [Judas Code (Vita), Star Ocean Anamnesis (Android), Star Ocean 4 (PC)] */
static int parse_aac_v3(STREAMFILE* sf, aac_header* aac) {
    off_t offset, size, test_offset;
    int target_subsong = sf->stream_index;

    /* base header */
    /* 0x00: id */
    /* 0x04: size */
    /* 0x10: config? (0x00020100/0x00020002/0x00020301/etc) */
    /* 0x14: platform ("VITA"=Vita, "DRD "=Android, "MSPC"=PC, "PS4 "=PS4) */

    /* offsets table: offset + flag? + size + align? */
    offset = read_u32le(0x20, sf); /* "AAOB" table (audio object?) */
    /* 0x30: "VRCB" table (some cue/config? related to subsongs? may be empty) */
    /* 0x40: "WAVB" table (wave body, has offset + size per stream then data, not needed since offsets are elsewhere too) */

    if (!is_id32le(offset + 0x00, sf, "AAOB"))
        goto fail;
    size = read_u32le(offset + 0x04, sf);

    if (target_subsong == 0) target_subsong = 1;
    aac->total_subsongs = 0;

    /* AAOB may point to N AAO (headers) in SFX/voice packs, seems signaled with flag 0x80 at AAOB+0x10
     * but there is no subsong count or even max size (always 0x1000?) */
    {
        for (test_offset = offset + 0x20; offset + size; test_offset += 0x10) {
            uint32_t entry_offset = read_u32le(test_offset + 0x00, sf);
            /* 0x04: entry size */

            if (entry_offset == get_id32be("AAO ")) /* reached end */
                break;

            if (entry_offset) { /* often 0 */
                aac->total_subsongs++;
                if (aac->total_subsongs == target_subsong) {
                    offset += entry_offset;
                }
            }
        }
    }

    if (target_subsong < 0 || target_subsong > aac->total_subsongs || aac->total_subsongs < 1) goto fail;

    if (!is_id32le(offset + 0x00, sf, "AAO "))
        goto fail;


    /* AAO section has offsets to "PLBK" chunk (?) and "WAVE" (header) */
    /* 0x14: PLBK offset */
    offset += read_u32le(offset + 0x18, sf); /* WAVE offset */
    if (!is_id32le(offset + 0x00, sf, "WAVE"))
        goto fail;
    offset += 0x10;

    /* 0x00: 0x00/01/01CC0000? */
    aac->codec          =    read_u8(offset + 0x04, sf);
    aac->channels       =    read_u8(offset + 0x05, sf);
    /* 0x06: 0x01? */
    /* 0x07: 0x10? (rarely 0x00) */
    aac->sample_rate    = read_s32le(offset + 0x08, sf);
    aac->stream_size    = read_u32le(offset + 0x0C, sf); /* usable size (without padding) */
    /* 0x10-1c: null  */
    aac->stream_offset  = read_u32le(offset + 0x20, sf); /* absolute */
    /* 0x24: data size (with padding) */
    /* 0x28: null */
    /* 0x2c: null */
    aac->loop_start     = read_u32le(offset + 0x30, sf); /* table positions(?) in OGG */
    aac->loop_end       = read_u32le(offset + 0x34, sf);
    /* 0x38: ? in OGG */
    aac->num_samples    = read_s32le(offset + 0x3c, sf); /* OGG only */
    aac->extra_offset   = offset + 0x40; /* codec specific */
    /* may have seek tables or other stuff per codec */

    aac->loop_flag = (aac->loop_end > 0);

    return 1;
fail:
    return 0;
}

static int parse_aac(STREAMFILE* sf, aac_header* aac) {
    int ok = 0;

    /* try variations as format evolved over time
     * chunk headers are always: id + size + null + null (ids in machine endianness) */

    ok = parse_aac_v1(sf, aac);
    if (ok) return 1;

    ok = parse_aac_v2(sf, aac);
    if (ok) return 1;

    ok = parse_aac_v3(sf, aac);
    if (ok) return 1;

    return 0;
}
