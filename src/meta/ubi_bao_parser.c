#include "../util/endianness.h"
#include "../util/log.h"
#include "../util/reader_helper.h"

#include "ubi_bao_parser.h"

// header classes, also similar to SB types:
static const ubi_bao_type_t type_map[BAO_MAX_TYPES] = {
    TYPE_NONE,      //00: not set
    TYPE_AUDIO,     //01: single audio (samples, channels, bitrate, samples+size, etc)
    TYPE_IGNORED,   //02: play chain with config? (ex. silence + audio, or rarely audio 2ch intro + layer 4ch body)
    TYPE_IGNORED,   //03: unknown chain
    TYPE_IGNORED,   //04: random (count, etc) + BAO IDs and float probability to play, may chain to other randoms
    TYPE_SEQUENCE,  //05: sequence (count, etc) + BAO IDs and unknown data
    TYPE_LAYER,     //06: layer (count, etc) + layer headers
    TYPE_IGNORED,   //07: unknown chain
    TYPE_SILENCE,   //08: silence (duration, etc)
    TYPE_IGNORED,   //09: silence with config? (channels, sample rate, etc), extremely rare [Shaun White Skateboarding (Wii)]
    //TYPE_NONE     //10+
};

//*************************************************************************************************

// some kind of points or parameter table, rare and mainly seen in chains (FC4 002358ee.spk, 0021bd69.spk, etc)
static bool parse_parameters_v2b(ubi_bao_header_t* bao, reader_t* r) {
    if (bao->cfg.engine_version < 0x2B00)
        return true;

    int count = reader_s32(r);
    if (count != 0 && count != 2) {
        VGM_LOG("UBI BAO: unexpected parameter count %i\n", count);
        return false;
    }

    for (int i = 0; i < count; i++) {
        uint32_t label_size = reader_u32(r);
        reader_skip(r, label_size); // TempoBPM, TempoTimeSig
    }

    int count2 = reader_s32(r);
    if (count != count2) {
        VGM_LOG("UBI BAO: unexpected parameter count2 %i\n", count2);
        return false;
    }
    
    reader_x32(r); // flag 1

    uint32_t points_size = reader_u32(r);
    if (points_size) {
        reader_skip(r, points_size); // 0x14 x2 (LE fields?)
    }

    return true;
}


// base 0x1c part for all baos
static bool parse_base_v29(ubi_bao_header_t* bao, reader_t* r) {
    reader_x32(r); // version
    reader_skip(r, 0x10); // 128-bit hash
    reader_x32(r); // bao class
    reader_x32(r); // fixed 2?

    return true;
}

// common 0x30 to all "header" class BAOs
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

    parse_parameters_v2b(bao, r);

    bao->loop_flag = reader_s32(r);

    reader_x32(r); // flag 1 / 0 (v0029?)
    reader_x32(r); // null
    reader_x32(r); // full stream size (ex. header xma chunk + stream size), null if loop_flag not set
    reader_x32(r); // null

    reader_x32(r); // null / original rate? (rare)
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
        bao->is_prefetch = reader_s32(r);
        bao->prefetch_size = reader_u32(r);

        reader_x32(r); // stream offset (always 0x1C = header_skip)
        bao->stream_id = reader_u32(r);
        uint32_t repeat_id = reader_u32(r); // prefetch id?
        reader_x32(r); // -1

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
        reader_skip(r, bao->extradata_size); // mp3 info, atrac9 config, xma header in memory BAOs, etc
    }

    int samples1 = reader_s32(r);
    reader_x32(r); // samples1 data size

    int samples2 = reader_s32(r);
    reader_x32(r); // samples2 data size (0 if not set, sometimes negative in xma?)

    if (bao->cfg.engine_version >= 0x2B00) {
        reader_x32(r); // null
    }


    /* post process */
    //TODO move?
    if (bao->loop_flag) {
        bao->loop_start  = samples1;
        bao->num_samples = samples1 + samples2;
    }
    else {
        bao->num_samples = samples1;
    }

    return true;
}

static bool parse_type_sequence_v29(ubi_bao_header_t* bao, reader_t* r) {

    reader_x32(r); // fixed? 0x3C2E0733
    reader_x32(r); // null

    parse_parameters_v2b(bao, r);

    reader_x32(r); // flag 0/1 (loop related) //TODO: sequence single?
    reader_x32(r); // null

    reader_x32(r); // null
    reader_x32(r); // null
    reader_x32(r); // flag 1
    reader_x32(r); // null

    reader_x32(r); // segments?
    reader_x32(r); // null
    reader_x32(r); // null
    reader_x32(r); // null

    reader_x32(r); // duration f32
    reader_x32(r); // null
    reader_x32(r); // null
    reader_x32(r); // null

    reader_x32(r); // null
    bao->sequence_count = reader_s32(r);

    if (bao->sequence_count > BAO_MAX_CHAIN_COUNT) {
        VGM_LOG("UBI BAO: incorrect sequence count of %i\n", bao->sequence_count);
        return false;
    }

    for (int i = 0; i < bao->sequence_count; i++) {
        bao->sequence_chain[i] = reader_u32(r);
        reader_x32(r); // flag 0/1 (loop related?)
        reader_x32(r); // flag 0/1 (loop related?)
        reader_x32(r); // low value

        reader_x32(r); // chain duration f32
    }

    if (bao->cfg.engine_version >= 0x2B00) {
        reader_x32(r); // flag 1
    }

    return true;
}

static bool parse_type_layer_v29(ubi_bao_header_t* bao, reader_t* r) {

    reader_x32(r); // fixed? 0xF2D3BBD4
    int layer_ids = reader_s32(r);
    for (int i = 0; i < layer_ids; i++) {
        reader_x32(r); // bao id (original/internal?)
    }

    parse_parameters_v2b(bao, r);

    bao->loop_flag = reader_s32(r); // full loops
    reader_x32(r); // null
    reader_x32(r); // layers?
    reader_x32(r); // null

    reader_x32(r); // bao id? (shared in multiple BAOs)
    reader_x32(r); // bao id? low value?
    bao->stream_size = reader_u32(r); // full size (ex. prefetch + stream)
    reader_x32(r); // null

    reader_x32(r); // flag 1 / 0 (less common)
    bao->is_stream = reader_s32(r);

    if (bao->is_stream) {
        bao->is_prefetch = reader_s32(r);
        bao->prefetch_size = reader_u32(r);

        reader_x32(r); // stream offset (always 0x1C = header_skip)
        reader_x32(r); // -1
        reader_x32(r); // -1 or rarely bao id?
        reader_x32(r); // -1
    }
    else {
        reader_x32(r); // -1
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
    reader_x32(r); // bao id? low value? (shared in multiple BAOs)
    if (bao->cfg.engine_version >= 0x2B00) {
        reader_x32(r); // null
    }
    bao->layer_count = reader_s32(r);

    if (bao->layer_count > BAO_MAX_LAYER_COUNT) {
        VGM_LOG("UBI BAO: incorrect layer count of %i\n", bao->layer_count);
        return false;
    }

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

        reader_x32(r); // related to layer size?
        reader_x32(r); // null or some config (64000)
        reader_x32(r); // flags? (00, 01, 02, 06)
        reader_x32(r); // flag 1

        layer->extradata_size = reader_u32(r);
        if (layer->extradata_size) {
            layer->extradata_offset = r->offset;
            reader_skip(r, layer->extradata_size); // mp3 info, atrac9 config, etc
        }
    }

    if (bao->cfg.engine_version < 0x2A00) {
        bao->is_inline = reader_s32(r);
        reader_x32(r); // flag 1
        bao->inline_size = reader_u32(r);
    }
    else if (bao->cfg.engine_version < 0x2B00) {
        bao->inline_size = reader_u32(r);
        bao->is_inline = reader_s32(r);
        reader_x32(r); // null
        reader_x32(r); // null
    }
    else {
        bao->inline_size = reader_u32(r);
        reader_x32(r); // inline_id? (-1 if not set, same as stream_id below)
        bao->is_inline = reader_s32(r);
        reader_x32(r); // null
        reader_x32(r); // null
    }

    if (bao->inline_size) {
        bao->inline_offset = r->offset;
        reader_skip(r, bao->inline_size); // codec data
    }

    // footer (stream_id equals header_id and is ignored if stream_flag is not set)
    if (bao->cfg.engine_version <= 0x2900) {
        bao->stream_id = reader_u32(r);
    }
    else {
        reader_x32(r); // null
        reader_x32(r); // flag 1
        reader_x32(r); // null
        bao->stream_id = reader_u32(r);
        reader_x32(r); // hash?
    }

    if (bao->cfg.engine_version >= 0x2B00) {
        reader_x32(r); // null
    }

    return true;
}

// generally only parsed if found in layers (very rare)
static bool parse_type_silence_v29(ubi_bao_header_t* bao, reader_t* r) {

    if (bao->cfg.engine_version > 0x2900) {
        VGM_LOG("UBI BAO: silence_v29 not implemented\n");
        return false;
    }

    reader_x32(r); // fixed? 0xFF4F734A
    reader_x32(r); // flag 1
    reader_x32(r); // bao id?
    bao->silence_duration = reader_f32(r);

    reader_x32(r); // bao id?

    return true;
}

// Parses a 0x00290000+ version BAOs. Unlike previous versions there are many variable sized-fields
// so instead of offset config we have feature flags.
static bool parse_header_v29(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    reader_t r = {0};
    reader_setup(&r, sf, offset, bao->cfg.big_endian);

    if (!parse_base_v29(bao, &r))
        return false;
    if (!parse_common_v29(bao, &r))
        return false;

    if (bao->header_type < BAO_MAX_TYPES) {
        bao->type = type_map[bao->header_type];
    }

    bool ok = false;
    switch(bao->type) {
        case TYPE_AUDIO:    ok = parse_type_audio_v29(bao, &r); break;
        case TYPE_SEQUENCE: ok = parse_type_sequence_v29(bao, &r); break;
        case TYPE_LAYER:    ok = parse_type_layer_v29(bao, &r); break;
        case TYPE_SILENCE:  ok = parse_type_silence_v29(bao, &r); break;
        default: ok = false;
    }

    if (!ok)
        return false;

    bao->header_size = (uint32_t)(r.offset - offset);

    //;VGM_LOG("UBI BAO: header v29 at %x\n", r.offset);
    return true;
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
        VGM_LOG("UBI BAO: incorrect sequence count of %i\n", bao->sequence_count);
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
        VGM_LOG("UBI BAO: incorrect layer count of %i\n", bao->layer_count);
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

        // check that layers were parsed correctly
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

            // samples can be +-1 between layers
            // uncommonly channels may vary per layer [Rayman Raving Rabbids: TV Party (Wii) ex. 0x22000cbc.pk]
        }
    }


    /* set codec */
    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == CODEC_NONE) {
        VGM_LOG("UBI BAO: unknown codec %x at %x\n", bao->stream_type, bao->header_offset);
        return false;
    }

    // no apparent flag
    if (bao->type == TYPE_LAYER && bao->codec == RAW_AT3) {
        if (bao->cfg.layer_default_subtype)
            bao->stream_subtype = bao->cfg.layer_default_subtype;
        else
            bao->stream_subtype = 1;
    }

    //TODO: loop flag only?
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

    // inline prefetch + stream = ok
    // inline memory = ok
    // inline stream only or inline prefetch only = ???
    if (bao->is_inline && ((bao->is_stream && !bao->is_prefetch) || (!bao->is_stream && bao->is_prefetch))) {
        VGM_LOG("UBI BAO: unexpected inline stream at %x\n", bao->header_offset);
        return false;
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

    // skip non-header classes but don't error to signal "bank has no subsongs"
    if (bao_class != 0x20000000)
        return true;

    uint32_t h_offset = offset + bao->cfg.header_skip;
    header_type = read_u32(h_offset + bao->cfg.header_type, sf);
    if (header_type > 9) {
        VGM_LOG("UBI BAO: unknown type %x at %x\n", header_type, (int)offset);
        return false;
    }

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
