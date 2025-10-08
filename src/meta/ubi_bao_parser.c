#include "../util/endianness.h"
#include "../util/log.h"

#include "ubi_bao_parser.h"


static bool parse_type_audio(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* audio header */
    bao->type = TYPE_AUDIO;

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


    uint32_t s_offset = h_offset;
    if (bao->cfg.audio_extradata_size) {
        // later versions have extradata before samples
        uint32_t extradata_size = read_s32(h_offset + bao->cfg.audio_extradata_size, sf);
        s_offset += extradata_size;
    }

    if (bao->loop_flag) {
        bao->loop_start  = read_s32(s_offset + bao->cfg.audio_num_samples, sf);
        bao->num_samples = read_s32(s_offset + bao->cfg.audio_num_samples2, sf) + bao->loop_start;
    }
    else {
        bao->num_samples = read_s32(s_offset + bao->cfg.audio_num_samples, sf);
    }

    bao->stream_type = read_s32(h_offset + bao->cfg.audio_stream_type, sf);
    if (bao->cfg.audio_stream_subtype)
        bao->stream_subtype = read_s32(h_offset + bao->cfg.audio_stream_subtype, sf);

    return true;
}

static bool parse_type_sequence(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* sequence chain */
    bao->type = TYPE_SEQUENCE;
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

static bool parse_type_layer(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* audio header */
    bao->type = TYPE_LAYER;
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
  //bao->channels       = read_s32(table_offset + bao->cfg.layer_channels, sf);
    bao->sample_rate    = read_s32(table_offset + bao->cfg.layer_sample_rate, sf);
    bao->stream_type    = read_s32(table_offset + bao->cfg.layer_stream_type, sf);
    bao->num_samples    = read_s32(table_offset + bao->cfg.layer_num_samples, sf);

    //TODO:
    if (bao->cfg.layer_stream_subtype)
        bao->stream_subtype = 1;
    //    bao->stream_subtype = read_s32(h_offset + bao->cfg.layer_stream_subtype, sf);

    for (int i = 0; i < bao->layer_count; i++) {
        int channels    = read_s32(table_offset + bao->cfg.layer_channels, sf);
        int sample_rate = read_s32(table_offset + bao->cfg.layer_sample_rate, sf);
        int stream_type = read_s32(table_offset + bao->cfg.layer_stream_type, sf);
        int num_samples = read_s32(table_offset + bao->cfg.layer_num_samples, sf);
        if (bao->sample_rate != sample_rate || bao->stream_type != stream_type) {
            VGM_LOG("UBI BAO: layer headers don't match at %x\n", table_offset);

            if (!bao->cfg.layer_ignore_error) {
                return false;
            }
        }

        // uncommonly channels may vary per layer [Rayman Raving Rabbids: TV Party (Wii) ex. 0x22000cbc.pk]
        bao->layer_channels[i] = channels;

        // can be +-1
        if (bao->num_samples != num_samples && bao->num_samples + 1 == num_samples) {
            bao->num_samples -= 1;
        }

        table_offset += bao->cfg.layer_entry_size;
    }

    return true;
}

static bool parse_type_silence(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_f32_t read_f32 = get_read_f32(bao->cfg.big_endian);

    /* silence header */
    bao->type = TYPE_SILENCE;
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
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    uint8_t header_format   = read_u8   (offset + 0x00, sf); // 0x01: atomic, 0x02: package
    uint32_t header_version = read_u32be(offset + 0x00, sf);
    if ((bao->cfg.version  & 0x00FFFF00) != (header_version & 0x00FFFF00) || header_format > 0x03) {
        // Avatar The Game has small variations, probably fine for the engine
        VGM_LOG("UBI BAO: mayor version header mismatch at %x\n", (uint32_t)offset);
        return false;
    }

    bao->header_offset  = offset;

    /* - base part in early versions:
     * 0x00: version ID
     * 0x04: header size (usually 0x28, rarely 0x24), can be LE unlike other fields (ex. Assassin's Creed PS3, but not in all games)
     * 0x08(10): GUID, or id-like fields in early versions
     * 0x18: null
     * 0x1c: null
     * 0x20: class
     * 0x24: config/version? (0x00/0x01/0x02), removed in some versions
     * (payload starts)
     *
     * - base part in later versions:
     * 0x00: version ID
     * 0x04(10): GUID
     * 0x14: class
     * 0x18: config/version? (0x02)
     * (payload starts)
     */

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

    switch(bao->header_type) {
        case 0x01:
            if (!parse_type_audio(bao, offset, sf))
                return false;
            break;
        case 0x05:
            if (!parse_type_sequence(bao, offset, sf))
                return false;
            break;
        case 0x06:
            if (!parse_type_layer(bao, offset, sf))
                return false;
            break;
        case 0x08:
            if (!parse_type_silence(bao, offset, sf))
                return false;
            break;
        default:
            VGM_LOG("UBI BAO: unknown header type at %x\n", (uint32_t)offset);
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
