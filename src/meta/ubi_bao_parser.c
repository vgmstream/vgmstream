#include "../util/endianness.h"
#include "../util/log.h"
#include "../util/reader_helper.h"

#include "ubi_bao_parser.h"

// header classes, also similar to SB types:
static const ubi_bao_type_t type_map[] = {
    TYPE_NONE,      //00: not set
    TYPE_AUDIO,     //01: single audio (samples, channels, bitrate, samples+size, etc)
    TYPE_NONE,      //02: play chain with config? (ex. silence + audio, or rarely audio 2ch intro + layer 4ch body)
    TYPE_NONE,      //03: unknown chain
    TYPE_NONE,      //04: random (count, etc) + BAO IDs and float probability to play
    TYPE_SEQUENCE,  //05: sequence (count, etc) + BAO IDs and unknown data
    TYPE_LAYER,     //06: layer (count, etc) + layer headers
    TYPE_NONE,      //07: unknown chain
    TYPE_SILENCE,   //08: silence (duration, etc)
    TYPE_NONE,      //09: silence with config? (channels, sample rate, etc), extremely rare [Shaun White Skateboarding (Wii)]
};

//*************************************************************************************************

// base 0x1c part for all baos
static bool parse_base_v29(ubi_bao_header_t* bao, reader_t* r) {
    reader_x32(r); // version
    reader_skip(r, 0x10); // 128-bit hash
    reader_x32(r); // bao class
    reader_x32(r); // fixed 2?

    return true;
}

// common to all "header" class BAOs
static bool parse_common_v29(ubi_bao_header_t* bao, reader_t* r) {
    reader_x32(r); // fixed? 0x17CE46D9
    bao->header_id = reader_u32(r);
    reader_x32(r); // -1
    reader_x32(r); // value (0xC0xx0000)

    reader_x32(r); // null
    reader_x32(r); // null
    reader_x32(r); // flag 1 (v002b) 0 (v002a)
    reader_x32(r); // null / -1

    reader_x32(r); // null
    reader_x32(r); // null
    reader_x32(r); // flag 1 / 0 (v0029?)
    bao->header_type = reader_u32(r); // bao type

    return true;
}

static bool parse_type_audio_v29(ubi_bao_header_t* bao, reader_t* r) {

    reader_x32(r); // fixed? 0x93B1ECE5
    reader_x32(r); // flag 1/2
    reader_x32(r); // bao id?
    bao->loop_flag = reader_s32(r);

    if (bao->cfg.audio_flag_2b) {
        reader_x32(r); // null
        reader_x32(r); // flag 1
        reader_x32(r); // null
        reader_x32(r); // null
    }

    reader_x32(r); // flag 1 / 0 (v0029)
    reader_x32(r); // null
    reader_x32(r); // full stream size (ex. header xma chunk + stream size), null if loop_flag not set
    reader_x32(r); // null

    reader_x32(r); // null
    reader_x32(r); // null
    bao->stream_type = reader_s32(r);
    bao->channels = reader_s32(r);

    bao->sample_rate = reader_s32(r);
    reader_x32(r); // bitrate
    bao->stream_size = reader_u32(r);
    reader_x32(r); // null

    reader_x32(r); // null
    bao->is_stream = reader_s32(r);

    if (bao->is_stream) {
        int flag = reader_s32(r); // null but possibly prefetch flag (compared to layers)
        reader_x32(r); // null

        reader_x32(r); // stream offset (always 0x1C = header_skip)
        bao->stream_id = reader_u32(r);
        uint32_t repeat_id = reader_u32(r); // prefetch id?
        reader_x32(r); // -1

        if (flag != 0) {
            VGM_LOG("UBI BAO: prefetch flag found\n");
            return false;
        }

        if (bao->stream_id != repeat_id) {
            VGM_LOG("UBI BAO: audio stream id mismatch %08x != %08x\n", bao->stream_id, repeat_id);
            return false;
        }
    }
    else {
        bao->stream_id = reader_u32(r); // memory id
    }

    int strings_size = reader_u32(r);
    if (strings_size) {
        strings_size = align_size_to_block(strings_size, 0x04);
        reader_skip(r, strings_size); // strings for cues
    }

    int cues_count = reader_s32(r);
    if (cues_count) {
        int cues_size = cues_count * 0x04;
        reader_skip(r, cues_size); // cues with float points
    }

    reader_x32(r); // -1
    reader_x32(r); // bao id?
    reader_x32(r); // low value, atrac9 related? extradata version? (16, 17)
    reader_x32(r); // flag 1

    bao->extradata_size = reader_u32(r);
    if (bao->extradata_size) {
        bao->extradata_offset = r->offset;
        reader_skip(r, bao->extradata_size); // atrac9 config, xma header in memory BAOs, etc
    }

    int samples1 = reader_s32(r);
    reader_x32(r); // samples1 data size

    int samples2 = reader_s32(r);
    reader_x32(r); // samples2 data size (0 if not set, sometimes negative in xma?)

    if (bao->cfg.audio_flag_2b) {
        reader_x32(r); // null
    }


    /* post process */
    //TODO move
    if (bao->loop_flag) {
        bao->loop_start  = samples1;
        bao->num_samples = samples1 + samples2;
    }
    else {
        bao->num_samples = samples1;
    }

    ;VGM_LOG("UBI BAO: header v29 at %x\n", r->offset);
    return true;
}

static bool parse_type_sequence_v29(ubi_bao_header_t* bao, reader_t* r) {
#if 0
    // fixed    
    // null
    // flag 1 (sequence loop?)
    // null

    // null
    // null
    // flag 1 (sequence loop?)
    // null

    // segments?
    // null
    // null
    // null

    // duration f32
    // null
    // null
    // null

    // null
    // segments
    for (int i = 0; i < bao->sequence_count; i++) {
        // id
        // null
        // null
        // format

        // duration f32
    }
#endif
    return false;
}

static bool parse_type_layer_v29(ubi_bao_header_t* bao, reader_t* r) {

    reader_x32(r); // fixed? 0xF2D3BBD4
    int layer_ids = reader_s32(r);
    for (int i = 0; i < layer_ids; i++) {
        reader_x32(r); // bao id
    }

    bao->loop_flag = reader_s32(r); // full loops
    reader_x32(r); // null
    reader_x32(r); // layers?
    reader_x32(r); // null

    reader_x32(r); // bao id? (shared in multiple BAOs)
    reader_x32(r); // low value (close to sample rate)
    bao->stream_size = reader_u32(r); // full size (ex. prefetch + stream)
    reader_x32(r); // null

    reader_x32(r); // flag 1
    bao->is_stream = reader_s32(r);

    if (bao->is_stream) {
        bao->is_prefetch = reader_s32(r);
        bao->prefetch_size = reader_u32(r);

        reader_x32(r); // stream offset (always 0x1C = header_skip)
        reader_x32(r); // -1
        reader_x32(r); // -1 or rarely bao id?
        reader_x32(r); // -1
    }

    reader_x32(r); // -1

    int strings_size = reader_u32(r);
    if (strings_size) {
        strings_size = align_size_to_block(strings_size, 0x04);
        reader_skip(r, strings_size); // strings for cues
    }

    int cues_count = reader_s32(r);
    if (cues_count) {
        int cues_size = cues_count * 0x04;
        reader_skip(r, cues_size); // cues with float points
    }

    reader_x32(r); // -1
    reader_x32(r); // bao id? (shared in multiple BAOs)
    bao->layer_count = reader_s32(r);

    for (int i = 0; i < bao->layer_count; i++) {
        ubi_bao_layer_t* layer = &bao->layer[i];

        layer->sample_rate = reader_s32(r);
        layer->channels    = reader_s32(r);
        layer->stream_type = reader_s32(r);
        reader_x32(r); // -1
        
        reader_x32(r); // null
        reader_x32(r); // null
        reader_x32(r); // value / null
        layer->num_samples = reader_s32(r);

        reader_x32(r); // layer size?
        reader_x32(r); // null
        reader_x32(r); // null / 06 (uncommon)
        reader_x32(r); // flag 1

        layer->extradata_size = reader_u32(r);
        if (bao->extradata_size) {
            layer->extradata_offset = r->offset;
            reader_skip(r, bao->extradata_size); // atrac9 config, xma header in memory BAOs, etc
        }
    }


    reader_x32(r); // inline flag
    reader_x32(r); // flag 1
    bao->inline_size = reader_u32(r);
    if (bao->inline_size) {
        bao->inline_offset = r->offset;
        reader_skip(r, bao->inline_size); // typically ubi-ima layers
    }

    bao->stream_id = reader_u32(r); // same as header_id, ignored if stream_flag is not set

    ///TODO: recheck v2a+
    //if v2a:
    //  memory_size (null = no memory)
    //  flag 1 (memory flag?)
    //  null
    //  null
    //
    //  memory data start (if any)
    //
    //  null
    //  flag 1
    //  null
    //  value
    //  high value

    ;VGM_LOG("UBI BAO: header v29 at %x\n", r->offset);
    return true;
}

static bool parse_type_silence_v29(ubi_bao_header_t* bao, reader_t* r) {
    VGM_LOG("UBI BAO: silence_v29 not implemented\n");
    return false;
}

// Parses a 0x00290000+ version BAOs. Unlike previous versions there are many variable sized-fields
// so instead of offset config we have feature flags.
static bool parse_header_v29(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    reader_t r = {0};
    reader_setup(&r, sf, offset, 0, bao->cfg.big_endian);

    if (!parse_base_v29(bao, &r))
        return false;
    if (!parse_common_v29(bao, &r))
        return false;

    if (bao->header_type < BAO_MAX_TYPES) {
        bao->type = type_map[bao->header_type];
    }

    switch(bao->type) {
        case TYPE_AUDIO: return parse_type_audio_v29(bao, &r); break;
        case TYPE_SEQUENCE: return parse_type_sequence_v29(bao, &r); break;
        case TYPE_LAYER: return parse_type_layer_v29(bao, &r); break;
        case TYPE_SILENCE: return parse_type_silence_v29(bao, &r); break;
        default:
            return false;
    }

    return false;
}

//*************************************************************************************************

static bool parse_type_audio_cfg(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->stream_size = read_u32(h_offset + bao->cfg.audio_stream_size, sf);
    bao->stream_id   = read_u32(h_offset + bao->cfg.audio_stream_id, sf);
    bao->is_stream   = read_s32(h_offset + bao->cfg.audio_stream_flag, sf) & bao->cfg.audio_stream_and;
    bao->loop_flag   = read_s32(h_offset + bao->cfg.audio_loop_flag, sf) & bao->cfg.audio_loop_and;
    bao->channels    = read_s32(h_offset + bao->cfg.audio_channels, sf);
    bao->sample_rate = read_s32(h_offset + bao->cfg.audio_sample_rate, sf);

    /* extra cue table, rare (found with XMA1/DSP) [Beowulf (X360), We Dare (Wii)] */
    uint32_t cues_size = 0;
    if (bao->cfg.audio_cue_count) {
        cues_size += read_u32(h_offset + bao->cfg.audio_cue_count, sf) * 0x08;
    }
    if (bao->cfg.audio_cue_labels) {
        cues_size += read_u32(h_offset + bao->cfg.audio_cue_labels, sf);
    }
    bao->extra_size = cues_size;

    /* prefetch data is in another internal BAO right after the base header */
    if (bao->cfg.audio_prefetch_size) {
        bao->prefetch_size = read_u32(h_offset + bao->cfg.audio_prefetch_size, sf);
        bao->is_prefetch = (bao->prefetch_size > 0);
    }


    if (bao->loop_flag) {
        bao->loop_start  = read_s32(h_offset + bao->cfg.audio_num_samples, sf);
        bao->num_samples = read_s32(h_offset + bao->cfg.audio_num_samples2, sf) + bao->loop_start;
    }
    else {
        bao->num_samples = read_s32(h_offset + bao->cfg.audio_num_samples, sf);
    }

    bao->stream_type = read_s32(h_offset + bao->cfg.audio_stream_type, sf);
    if (bao->cfg.audio_stream_subtype)
        bao->stream_subtype = read_s32(h_offset + bao->cfg.audio_stream_subtype, sf);

    return true;
}

static bool parse_type_sequence_cfg(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    if (bao->cfg.sequence_entry_size == 0) {
        VGM_LOG("UBI BAO: sequence entry size not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->sequence_loop   = read_s32(h_offset + bao->cfg.sequence_sequence_loop, sf);
    bao->sequence_single = read_s32(h_offset + bao->cfg.sequence_sequence_single, sf);
    bao->sequence_count  = read_s32(h_offset + bao->cfg.sequence_sequence_count, sf);
    if (bao->sequence_count > BAO_MAX_CHAIN_COUNT) {
        VGM_LOG("UBI BAO: incorrect sequence count\n");
        return false;
    }

    /* get chain in extra table */
    uint32_t table_offset = offset + bao->header_size;
    for (int i = 0; i < bao->sequence_count; i++) {
        uint32_t entry_id = read_u32(table_offset + bao->cfg.sequence_entry_number, sf);

        bao->sequence_chain[i] = entry_id;

        table_offset += bao->cfg.sequence_entry_size;
    }

    return true;
}

static bool parse_type_layer_cfg(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    if (bao->cfg.layer_entry_size == 0) {
        VGM_LOG("UBI BAO: layer entry size not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->layer_count    = read_s32(h_offset + bao->cfg.layer_layer_count, sf);
    bao->is_stream      = read_s32(h_offset + bao->cfg.layer_stream_flag, sf) & bao->cfg.layer_stream_and;
    bao->stream_size    = read_u32(h_offset + bao->cfg.layer_stream_size, sf);
    bao->stream_id      = read_u32(h_offset + bao->cfg.layer_stream_id, sf);
    if (bao->layer_count > BAO_MAX_LAYER_COUNT) {
        VGM_LOG("UBI BAO: incorrect layer count\n");
        return false;
    }

    if (bao->cfg.layer_prefetch_size) {
        bao->prefetch_size = read_u32(h_offset + bao->cfg.layer_prefetch_size, sf);
        bao->is_prefetch = (bao->prefetch_size > 0);
    }

    /* extra cue table (rare, has N variable-sized labels + cue table pointing to them) */
    uint32_t cues_size = 0;
    if (bao->cfg.layer_cue_labels) {
        cues_size += read_u32(h_offset + bao->cfg.layer_cue_labels, sf);
    }
    if (bao->cfg.layer_cue_count) {
        cues_size += read_u32(h_offset + bao->cfg.layer_cue_count, sf) * 0x08;
    }

    if (bao->cfg.layer_extra_size) {
        bao->extra_size = read_u32(h_offset + bao->cfg.layer_extra_size, sf);
    }
    else {
        bao->extra_size = cues_size + bao->layer_count * bao->cfg.layer_entry_size + cues_size;
    }

    /* get 1st layer header in extra table and validate all headers match */
    uint32_t table_offset = offset + bao->header_size + cues_size;

    //TODO:
    if (bao->cfg.layer_stream_subtype)
        bao->stream_subtype = 1;
    //    bao->stream_subtype = read_s32(h_offset + bao->cfg.layer_stream_subtype, sf);

    for (int i = 0; i < bao->layer_count; i++) {
        ubi_bao_layer_t* layer = &bao->layer[i];
        layer->channels    = read_s32(table_offset + bao->cfg.layer_channels, sf);
        layer->sample_rate = read_s32(table_offset + bao->cfg.layer_sample_rate, sf);
        layer->stream_type = read_s32(table_offset + bao->cfg.layer_stream_type, sf);
        layer->num_samples = read_s32(table_offset + bao->cfg.layer_num_samples, sf);

        table_offset += bao->cfg.layer_entry_size;
    }

    return true;
}

static bool parse_type_silence_cfg(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_f32_t read_f32 = get_read_f32(bao->cfg.big_endian);

    if (bao->cfg.silence_duration_float == 0) {
        VGM_LOG("UBI BAO: silence duration not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->silence_duration = read_f32(h_offset + bao->cfg.silence_duration_float, sf);
    if (bao->silence_duration <= 0.0f) {
        VGM_LOG("UBI BAO: bad duration %f at %x\n", bao->silence_duration, (uint32_t)offset);
        return false;
    }

    return true;
}


/* 0x00: version ID
 * 0x04: header size (usually 0x28, rarely 0x24), can be LE unlike other fields (ex. Assassin's Creed PS3, but not in all games)
 * 0x08(10): GUID, or id-like fields in early versions
 * 0x18: null
 * 0x1c: null
 * 0x20: class
 * 0x24: config/version? (0x00/0x01/0x02), removed in some versions
 * (payload starts)
 */
static bool parse_header_cfg(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->header_id      = read_u32(h_offset + bao->cfg.header_id, sf);
    bao->header_type    = read_u32(h_offset + bao->cfg.header_type, sf);

    bao->header_size    = bao->cfg.header_base_size;

    /* hack for games with smaller size than standard
     * (can't use lowest size as other games also have extra unused field) */
    if (bao->cfg.header_less_le_flag && !bao->cfg.big_endian) {
        bao->header_size -= 0x04;
    }
    /* detect extra unused field in PC/Wii
     * (could be improved but no apparent flags or anything useful) */
    else if (get_streamfile_size(sf) > offset + bao->header_size) {
        // may read next BAO version, layer header, cues, resource table size, etc, always > 1
        int32_t end_field = read_s32(offset + bao->header_size, sf);

        if (end_field == -1 || end_field == 0 || end_field == 1) { // some count?
            bao->header_size += 0x04;
        }
    }
    
    if (bao->header_type < BAO_MAX_TYPES) {
        bao->type = type_map[bao->header_type];
    }

    switch(bao->type) {
        case TYPE_AUDIO: return parse_type_audio_cfg(bao, offset, sf); break;
        case TYPE_SEQUENCE: return parse_type_sequence_cfg(bao, offset, sf); break;
        case TYPE_LAYER: return parse_type_layer_cfg(bao, offset, sf); break;
        case TYPE_SILENCE: return parse_type_silence_cfg(bao, offset, sf); break;
        default:
            return false;
    }

    return false;
}

//*************************************************************************************************

/* adjust some common values */
static bool parse_values(ubi_bao_header_t* bao) {

    if (bao->type == TYPE_SEQUENCE || bao->type == TYPE_SILENCE)
        return true;

    /* common validations */
    if (bao->stream_size == 0) {
        VGM_LOG("UBI BAO: unknown stream_size at %x\n", bao->header_offset);
        return false;
    }

    if (bao->stream_type >= BAO_MAX_CODECS) {
        VGM_LOG("UBI BAO: unknown stream_type at %x\n", bao->header_offset);
        return false;
    }

    // post-process layers
    if (bao->type == TYPE_LAYER) {
      //bao->channels       = bao->layer[0].channels; // loaded per layer
        bao->sample_rate    = bao->layer[0].sample_rate; //TODO: each layer has its own
        bao->stream_type    = bao->layer[0].stream_type;
        bao->num_samples    = bao->layer[0].num_samples;

        // uncommonly channels may vary per layer [Rayman Raving Rabbids: TV Party (Wii) ex. 0x22000cbc.pk]
        // samples can be +-1 between layers
        for (int i = 1; i < bao->layer_count; i++) {
            ubi_bao_layer_t* layer_curr = &bao->layer[i];
            ubi_bao_layer_t* layer_prev = &bao->layer[i-1];

            if (layer_curr->sample_rate != layer_prev->sample_rate || layer_curr->stream_type != layer_prev->stream_type) {
                VGM_LOG("UBI BAO: layer headers don't match\n");

                // sample rate mismatch, would need resampling
                if (!bao->cfg.layer_ignore_error) {
                    return false;
                }
            }
        }
    }


    /* set codec */
    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == CODEC_NONE) {
        VGM_LOG("UBI BAO: unknown codec %x at %x\n", bao->stream_type, bao->header_offset);
        return false;
    }

    //TODO: loop flag only?
    //TODO: put in PSX code?
    if (bao->type == TYPE_AUDIO && bao->codec == RAW_PSX && bao->cfg.v1_bao && bao->loop_flag) {
        bao->num_samples = bao->num_samples / bao->channels;
    }

    /* normalize base skips, as memory data (prefetch or not, atomic or package) can be
     * in a memory BAO after base header or audio layer BAO after the extra table */
    if (bao->stream_id == bao->header_id && (!bao->is_stream || bao->is_prefetch)) { // layers with memory data
        bao->memory_skip = bao->header_size + bao->extra_size;
        bao->stream_skip = bao->cfg.header_skip;
    }
    else {
        bao->memory_skip = bao->cfg.header_skip;
        bao->stream_skip = bao->cfg.header_skip;
    }

    if (!bao->is_stream && bao->is_prefetch) {
        VGM_LOG("UBI BAO: unexpected non-streamed prefetch at %x\n", bao->header_offset);
        //return false; //?
    }

    return true;
}


/* parse a single known header resource at offset (see config_bao for info) */
static bool parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {

    uint8_t header_format   = read_u8   (offset + 0x00, sf); // 0x01: atomic, 0x02: package
    uint32_t header_version = read_u32be(offset + 0x00, sf);
    if ((bao->cfg.version  & 0x00FFFF00) != (header_version & 0x00FFFF00) || header_format > 0x03) {
        // Avatar The Game has small variations, probably fine for the engine
        VGM_LOG("UBI BAO: mayor version header mismatch at %x\n", (uint32_t)offset);
        return false;
    }

    bao->header_offset  = offset;

    bool ok = false;
    switch(bao->cfg.parser) {
        case PARSER_1B: ok = parse_header_cfg(bao, sf, offset); break;
        case PARSER_29: ok = parse_header_v29(bao, sf, offset); break;
        default:
            VGM_LOG("UBI BAO: unknown parser\n");
            return false;
    }

    if (!ok) {
        VGM_LOG("UBI BAO: failed to parse header at %x\n", (uint32_t)offset);
        return false;
    }

    if (!parse_values(bao))
        return false;

    return true;
}

/* parse a full BAO, DARE's main audio format which can be inside other formats */
static bool parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong) {
    uint32_t bao_class, header_type;

    uint32_t bao_version = read_u32be(offset+0x00, sf); // force buffer read (check just in case it's optimized out)
    if (((bao_version >> 24) & 0xFF) > 0x02) {
        VGM_LOG("UBI BAO: wrong header type %x at %x\n", bao_version, (uint32_t)offset);
        return false;
    }

    ubi_bao_config_endian(&bao->cfg, sf, offset);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    bao_class = read_u32(offset + bao->cfg.bao_class, sf);
    if (bao_class & 0x0FFFFFFF) {
        VGM_LOG("UBI BAO: unknown class %x at %x\n", bao_class, (uint32_t)offset);
        return false;
    }

    bao->classes[(bao_class >> 28) & 0xF]++;
    if (bao_class != 0x20000000) // skip non-header classes
        return true;

    uint32_t h_offset = offset + bao->cfg.header_skip;
    header_type = read_u32(h_offset + bao->cfg.header_type, sf);
    if (header_type > 9) {
        VGM_LOG("UBI BAO: unknown type %x at %x\n", header_type, (int)offset);
        return false;
    }

    bao->types[header_type]++;
    if (!bao->cfg.allowed_types[header_type])
        return true;

    bao->total_subsongs++;
    if (target_subsong != bao->total_subsongs)
        return true;

    if (!parse_header(bao, sf, offset)) {
        VGM_LOG("UBI BAO: wrong header at %x\n", (uint32_t)offset);
        return false;
    }

    return true;
}


bool ubi_bao_parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    return parse_header(bao, sf, offset);
}

bool ubi_bao_parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong) {
    return parse_bao(bao, sf, offset, target_subsong);
}
