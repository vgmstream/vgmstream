#ifndef _FSB_FEV_H_
#define _FSB_FEV_H_
#include "meta.h"


typedef struct {
    uint32_t version;

    off_t name_offset;
    size_t name_size;

    /* internal */
    off_t offset;
} fev_header_t;


// TODO: find real version examples from games
#define FMOD_FEV_VERSION_7_0    0x00070000  // ?
#define FMOD_FEV_VERSION_8_0    0x00080000  // ?
#define FMOD_FEV_VERSION_9_0    0x00090000  // ?
#define FMOD_FEV_VERSION_10_0   0x000A0000  // ?
#define FMOD_FEV_VERSION_11_0   0x000B0000  // ?
#define FMOD_FEV_VERSION_12_0   0x000C0000  // ?
#define FMOD_FEV_VERSION_13_0   0x000D0000  // ?
#define FMOD_FEV_VERSION_14_0   0x000E0000  // ?
#define FMOD_FEV_VERSION_15_0   0x000F0000  // ?
#define FMOD_FEV_VERSION_16_0   0x00100000  // ?
#define FMOD_FEV_VERSION_18_0   0x00120000  // ?
#define FMOD_FEV_VERSION_19_0   0x00130000  // ?
#define FMOD_FEV_VERSION_20_0   0x00140000  // ?
#define FMOD_FEV_VERSION_22_0   0x00160000  // ?
#define FMOD_FEV_VERSION_23_0   0x00170000  // ?
#define FMOD_FEV_VERSION_24_0   0x00180000  // ?
#define FMOD_FEV_VERSION_25_0   0x00190000  // ?
#define FMOD_FEV_VERSION_26_0   0x001A0000  // ?
#define FMOD_FEV_VERSION_27_0   0x001B0000  // ?
#define FMOD_FEV_VERSION_28_0   0x001C0000  // ?
#define FMOD_FEV_VERSION_29_0   0x001D0000  // ?
#define FMOD_FEV_VERSION_30_0   0x001E0000  // ?
#define FMOD_FEV_VERSION_31_0   0x001F0000  // ?
#define FMOD_FEV_VERSION_32_0   0x00200000  // ?
#define FMOD_FEV_VERSION_34_0   0x00220000  // ?
#define FMOD_FEV_VERSION_36_0   0x00240000  // ?
#define FMOD_FEV_VERSION_37_0   0x00250000  // ?
#define FMOD_FEV_VERSION_38_0   0x00260000  // ?
#define FMOD_FEV_VERSION_39_0   0x00270000  // ?
#define FMOD_FEV_VERSION_40_0   0x00280000  // ?
#define FMOD_FEV_VERSION_41_0   0x00290000  // ?
#define FMOD_FEV_VERSION_42_0   0x002A0000  // ?
#define FMOD_FEV_VERSION_43_0   0x002B0000  // ?
#define FMOD_FEV_VERSION_44_0   0x002C0000  // Monster Jam (PS2)
#define FMOD_FEV_VERSION_45_0   0x002D0000  // ?
#define FMOD_FEV_VERSION_46_0   0x002E0000  // ?
#define FMOD_FEV_VERSION_49_0   0x00310000  // ?
#define FMOD_FEV_VERSION_50_0   0x00320000  // Monster Jam: Urban Assault (PS2)
#define FMOD_FEV_VERSION_52_0   0x00340000  // Stoked (X360)
#define FMOD_FEV_VERSION_56_0   0x00380000  // ?
#define FMOD_FEV_VERSION_57_0   0x00390000  // ?
#define FMOD_FEV_VERSION_58_0   0x003A0000  // Split/Second (PS3 QA Beta)
#define FMOD_FEV_VERSION_60_0   0x003C0000  // ?
#define FMOD_FEV_VERSION_61_0   0x003D0000  // Split/Second (PS3/X360/PC), Armored Core V (PS3), Supreme Commander 2 (PC)
#define FMOD_FEV_VERSION_62_0   0x003E0000  // ?
#define FMOD_FEV_VERSION_63_0   0x003F0000  // ?
#define FMOD_FEV_VERSION_64_0   0x00400000  // Brutal Legend (PC)
#define FMOD_FEV_VERSION_65_0   0x00410000  // ?
#define FMOD_FEV_VERSION_68_0   0x00440000  // ?
#define FMOD_FEV_VERSION_69_0   0x00450000  // ?


static inline uint32_t read_fev_u32(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t ret = read_u32le(fev->offset, sf);
    fev->offset += 0x04;
    return ret;
}

static inline uint16_t read_fev_u16(fev_header_t* fev, STREAMFILE* sf) {
    uint16_t ret = read_u16le(fev->offset, sf);
    fev->offset += 0x02;
    return ret;
}

static uint32_t read_fev_string(fev_header_t* fev, STREAMFILE* sf) {
    // strings are probably the only "sane" thing in FEVs, so enforce stricter checks
    uint32_t str_size;

    // string size includes null terminator
    str_size = read_fev_u32(fev, sf);
    if (str_size > 0x100) return 0; // arbitrary max
    if (!str_size) return 0x04; // total bytes read

    for (int i = 0; i < str_size - 1; i++) {
        // ASCII only for now, less lenient than read_str
        uint8_t c = read_u8(fev->offset++, sf);
        if (c < 0x20 || c > 0x7E) return 0;
    }
    if (read_u8(fev->offset++, sf) != 0x00)
        return 0;

    return str_size + 0x04;
}

static bool read_fev_uuid(fev_header_t* fev, STREAMFILE* sf) {
    // similarly to strings, uuids are one of the rare "sane" things to check for
    uint16_t uuid_seg3, uuid_seg4;

    uuid_seg3 = read_u16le(fev->offset + 0x06, sf);
    uuid_seg4 = read_u16be(fev->offset + 0x08, sf);

    if ((uuid_seg3 & 0xF000) != 0x4000)
        return false;
    if ((uuid_seg4 & 0xF000) < 0x8000 ||
        (uuid_seg4 & 0xF000) > 0xB000)
        return false;

    fev->offset += 0x10;
    return true;
}


static bool parse_fev_properties(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t properties, property_type;

    properties = read_fev_u32(fev, sf);
    for (int i = 0; i < properties; i++) {
        if (!read_fev_string(fev, sf)) // property name
            return false;

        // enum: 0 = int, 1 = float, 2 = string
        property_type = read_fev_u32(fev, sf);
        if (property_type < 0x00 || property_type > 0x02)
            return false;

        if (property_type != 0x02)
            fev->offset += 0x04;
        else if (!read_fev_string(fev, sf)) // property
            return false;
    }

    return true;
}


static bool parse_fev_category(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t categories;

    if (!read_fev_string(fev, sf)) // category name
        return false;
    // 0x00: volume
    // 0x04: pitch
    // 0x08: (v0x29+) max playbacks
    // 0x0C: (v0x29+) max playbacks flags
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_41_0)
        fev->offset += 0x08;

    categories = read_fev_u32(fev, sf);
    for (int i = 0; i < categories; i++) {
        if (!parse_fev_category(fev, sf))
            return false;
    }

    return true;
}


static bool parse_fev_event_sound(fev_header_t* fev, STREAMFILE* sf) {

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        fev->offset += 0x02; // name index?
    else if (!read_fev_string(fev, sf)) // sound name
        return false;

    // 0x00: x(?)
    // 0x04: width(?)
    // 0x08: (v0x1E+) more flags
    // 0x0C: flags
    // 0x10: (v0x1F+) loop count
    // 0x14: auto pitch flag
    // 0x18: auto pitch reference
    // 0x1C: (v0x24+) auto pitch zero
    // 0x20: fine tune
    // 0x24: (<v0x34) m_start_synch?
    // 0x28: (<v0x34) m_start_synch?
    // 0x2C: volume
    // 0x30: fade in
    // 0x34: fade out
    // 0x38: (v0x18+) fade in type
    // 0x3C: (v0x18+) fade out type
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_30_0)
        fev->offset += 0x04;
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_31_0)
        fev->offset += 0x04;
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_36_0)
        fev->offset += 0x04;
    fev->offset += 0x04;
    if (fev->version <  FMOD_FEV_VERSION_52_0)
        fev->offset += 0x08;
    fev->offset += 0x0C;
    if (fev->version >= FMOD_FEV_VERSION_24_0)
        fev->offset += 0x08;

    return true;
}


static bool parse_fev_event_envelope(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t points;

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        fev->offset += 0x04; // name index?
    else {
        if (!read_fev_string(fev, sf)) // envelope name
            return false;
        if (!read_fev_string(fev, sf)) // parent envelope name
            return false;
    }

    if (!read_fev_string(fev, sf)) // dsp unit name
        return false;

    // 0x00: dsp param index
    // 0x04: (v0x26+) flags
    // 0x08: (v0x39+) more flags
    // 0x0C: points
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_38_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_57_0)
        fev->offset += 0x04;
    points = read_fev_u32(fev, sf);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        fev->offset += points * 0x04; // idx
    else {
        fev->offset += points * 0x08; // x,y
        if (fev->version >= FMOD_FEV_VERSION_13_0)
            fev->offset += points * 0x04; // flags
    }
    // params/flags
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_26_0)
        fev->offset += 0x04;

    return true;
}


static bool parse_fev_event_complex(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t layers, params, sounds, envelopes, points;

    layers = read_fev_u32(fev, sf);
    for (int i = 0; i < layers; i++) {
        if (fev->version < FMOD_FEV_VERSION_39_0) {
            if (!read_fev_string(fev, sf)) // layer name
                return false;
        }

        // 0x00: uses software (u32), (v0x1D+) flags (u16)
        // 0x02: (v0x25+) priority (u16)
        // 0x06: param name (str), (v0x27+) param index (u16)
        // 0x08: sounds (u32), (v0x1D+) sounds (u16)
        // 0x08: envelopes (u32), (v0x1D+) envelopes (u16)
        fev->offset += (fev->version >= FMOD_FEV_VERSION_29_0) ? 0x02 : 0x04;
        if (fev->version >= FMOD_FEV_VERSION_37_0)
            fev->offset += 0x02;
        if (fev->version >= FMOD_FEV_VERSION_39_0)
            fev->offset += 0x02;
        else if (!read_fev_string(fev, sf)) // param name
            return false;
        if (fev->version >= FMOD_FEV_VERSION_29_0) {
            sounds    = read_fev_u16(fev, sf);
            envelopes = read_fev_u16(fev, sf);
        }
        else {
            sounds    = read_fev_u32(fev, sf);
            envelopes = read_fev_u32(fev, sf);
        }

        for (int j = 0; j < sounds; j++) {
            if (!parse_fev_event_sound(fev, sf))
                return false;
        }
        for (int j = 0; j < envelopes; j++) {
            if (!parse_fev_event_envelope(fev, sf))
                return false;
        }
    }

    params = read_fev_u32(fev, sf);
    for (int i = 0; i < params; i++) {
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            fev->offset += 0x04; // name index?
        else if (!read_fev_string(fev, sf)) // param name
            return false;

        if (fev->version <  FMOD_FEV_VERSION_18_0)
            fev->offset += 0x08; // removed min/max

        // 0x00: velocity
        // 0x04: range min original
        // 0x08: range max original
        // 0x0C: primary(?), (v0x10+) flags
        // 0x10: (v0x0B~v0x10) loop mode
        // 0x14: (v0x12+) seek speed
        // 0x18: envelopes
        // 0x1C: (v0x0C+) sustain points
        fev->offset += 0x10;
        if (fev->version >= FMOD_FEV_VERSION_11_0 &&
            fev->version <  FMOD_FEV_VERSION_16_0)
            fev->offset += 0x04;
        if (fev->version >= FMOD_FEV_VERSION_18_0)
            fev->offset += 0x04;
        fev->offset += 0x04;
        if (fev->version >= FMOD_FEV_VERSION_12_0) {
            points = read_fev_u32(fev, sf);
            fev->offset += points * 0x04;
        }
    }

    if (!parse_fev_properties(fev, sf))
        return false;

    return true;
}


static bool parse_fev_event_simple(fev_header_t* fev, STREAMFILE* sf) {

    fev->offset += 0x04; // flags
    if (!parse_fev_event_sound(fev, sf))
        return false;

    return true;
}


static bool parse_fev_event(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t event_type, categories;

    event_type = 0x08; // 0x08 = complex, 0x10 = simple
    if (fev->version >= FMOD_FEV_VERSION_52_0)
        event_type = read_fev_u32(fev, sf);

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        fev->offset += 0x04; // name index?
    else if (!read_fev_string(fev, sf)) // event name
        return false;

    if (fev->version >= FMOD_FEV_VERSION_58_0) {
        if (!read_fev_uuid(fev, sf))
            return false;
    }

    // 0x00: volume
    // 0x04: pitch
    // 0x08: (v0x1B+) pitch randomisation
    // 0x0C: (v0x20+) volume randomisation
    // 0x10: (v0x0A+) priority
    // 0x14: max playbacks
    // 0x18: (v0x38+) steal priority
    // 0x1C: mode (Big Endian flags?)
    // 0x20: min distance 3D
    // 0x24: max distance 3D
    // 0x28: (v0x45+) distance filtering
    // 0x2C: (v0x45+) dist filt center freq
    // 0x30: music(?), (v0x0F+) flags
    // 0x34: (v0x09+) speaker levels[8]
    // 0x54: (v0x09+) cone inside angle
    // 0x58: (v0x09+) cone outside angle
    // 0x5C: (v0x09+) cone outside volume
    // 0x60: (v0x0B+) max playbacks flags
    // 0x64: (v0x0B+) doppler factor 3D
    // 0x68: (v0x1C+) reverb dry level
    // 0x6C: (v0x0B+) reverb wet level
    // 0x70: (v0x12+) speaker spread 3D
    // 0x74: (v0x13+) fade in time
    // 0x78: (v0x13+) fade out time
    // 0x7C: (v0x2B+) spawn intensity
    // 0x80: (v0x2D+) spawn intensity randomisation
    // 0x84: (v0x16+) pan level 3D
    // 0x8C: (v0x44+) position randomisation 3D min
    // 0x90: (v0x28+) position randomisation 3D max
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_32_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_10_0)
        fev->offset += 0x04;
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_56_0)
        fev->offset += 0x04;
    fev->offset += 0x0C;
    if (fev->version >= FMOD_FEV_VERSION_69_0)
        fev->offset += 0x08;
    //if (fev->version >= FMOD_FEV_VERSION_15_0)
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_9_0)
        fev->offset += 0x2C; // 0x20 + 0x0C
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_28_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_18_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_19_0)
        fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_43_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_45_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_22_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_40_0)
        fev->offset += 0x04;

    if ((event_type & 0x18) == 0x08) {
        if (!parse_fev_event_complex(fev, sf))
            return false;
    }
    else if ((event_type & 0x18) == 0x10) {
        if (!parse_fev_event_simple(fev, sf))
            return false;
    }
    else {
        vgm_logi("FEV: Invalid event configuration\n");
        return false;
    }

    categories = read_fev_u32(fev, sf);
    for (int i = 0; i < categories; i++) {
        if (!read_fev_string(fev, sf))
            return false;
    }

    return true;
}


static bool parse_fev_event_category(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t event_groups, events;

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        fev->offset += 0x04; // name index?
    else if (!read_fev_string(fev, sf)) // event category name
        return false;

    if (fev->version >= FMOD_FEV_VERSION_23_0) {
        if (!parse_fev_properties(fev, sf))
            return false;
    }

    event_groups = read_fev_u32(fev, sf);
    events       = read_fev_u32(fev, sf);

    for (int i = 0; i < event_groups; i++) {
        if (!parse_fev_event_category(fev, sf))
            return false;
    }
    for (int i = 0; i < events; i++) {
        if (!parse_fev_event(fev, sf))
            return false;
    }

    return true;
}


static void parse_fev_sound_def_def(fev_header_t* fev, STREAMFILE* sf) {

    // 0x00: playlist settings
    // 0x04: (v0x22~v0x26) spawn intensity, spawn time min
    // 0x08: (<v0x22, v0x26+) spawn time max (???)
    // 0x0C: spawn time min
    // 0x10: spawn time max
    // 0x14: (v0x1B+) volume rand method
    // 0x18: volume random min
    // 0x1C: volume random max
    // 0x20: (v0x1B+) volume randomisation
    // 0x24: pitch
    // 0x28: (v0x1B+) pitch rand method
    // 0x2C: pitch random min
    // 0x30: pitch random max
    // 0x34: (v0x1B+) pitch randomisation
    // 0x38: (v0x3C+) pitch recalc mode
    // 0x3C: (v0x44+) position rand min
    // 0x40: (v0x2A+) position rand max
    // 0x44: (v0x3E+) trigger delay min
    // 0x48: (v0x3E+) trigger delay max
    // 0x4C: (v0x3F+) spawn count
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_34_0 &&
        fev->version <  FMOD_FEV_VERSION_38_0)
        fev->offset += 0x04;
    else
        fev->offset += 0x08;
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        fev->offset += 0x04;
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        fev->offset += 0x04;
    fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        fev->offset += 0x04;
    fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_60_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_42_0)
        fev->offset += 0x04;
    if (fev->version >= FMOD_FEV_VERSION_62_0)
        fev->offset += 0x08;
    if (fev->version >= FMOD_FEV_VERSION_63_0)
        fev->offset += 0x04;

}


static bool parse_fev(fev_header_t* fev, STREAMFILE* sf) {
    // initial research based on xoreos and adapted for other FEV versions
    // https://github.com/xoreos/xoreos/blob/master/src/sound/fmodeventfile.cpp
    // further research from FMOD::EventSystemI::load in Split/Second's fmod_event.dll
    // lastly also found this project by putting fmod_event.dll's func name in github search
    // https://github.com/barspinoff/bmod/blob/main/tools/fmod_event/src/fmod_eventsystemi.cpp
    uint32_t wave_banks, event_groups, sound_defs, languages = 1;
    int target_subsong = sf->stream_index;

    if (!is_id32be(0x00, sf, "FEV1"))
        return false;

    fev->version = read_u32le(0x04, sf);
    // fmod_event.dll rejects any version below 7.0
    if ((fev->version & 0xFF00FFFF) || fev->version < FMOD_FEV_VERSION_7_0)
        return false;

    if (target_subsong == 0)
        target_subsong = 1;
    target_subsong--;

    // by far the biggest issue with FEV is the lack of pointers to anything,
    // so everything has to be read in sequence until you get to stream names
    fev->offset = 0x08;

    // unknown 2 values at the beginning
    if (fev->version >= FMOD_FEV_VERSION_46_0)
        fev->offset += 0x04; // fmod_event.dll v0x3E still reads this
    if (fev->version >= FMOD_FEV_VERSION_50_0)
        fev->offset += 0x04; // fmod_event.dll v0x3E seeks over this

    // unknown array of 2 values per element
    if (fev->version >= FMOD_FEV_VERSION_64_0)
        fev->offset += read_fev_u32(fev, sf) * 0x08;

    // FEV bank name (v0x19+ per fmod_event.dll)
    if (fev->version >= FMOD_FEV_VERSION_25_0) {
        if (!read_fev_string(fev, sf))
            return false;
    }
    else return false; // TODO (v0x07~v0x18)


    wave_banks = read_fev_u32(fev, sf);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        languages = read_fev_u32(fev, sf);

    for (int i = 0; i < wave_banks; i++) {
        if (fev->version >= FMOD_FEV_VERSION_20_0)
            fev->offset += 0x04; // max streams
        fev->offset += 0x04; // flags?

        // hashes and "suffixes"?
        if (fev->version >= FMOD_FEV_VERSION_61_0) {
            for (int j = 0; j < languages; j++) {
                fev->offset += 0x08; // some hash
                if (fev->version >= FMOD_FEV_VERSION_65_0)
                    fev->offset += 0x04; // "fsb suffix"?
            }
        }

        if (!read_fev_string(fev, sf)) // wave bank name
            return false;
    }

    if (!parse_fev_category(fev, sf))
        return false;

    event_groups = read_fev_u32(fev, sf);
    for (int i = 0; i < event_groups; i++) {
        if (!parse_fev_event_category(fev, sf))
            return false;
    }

    if (fev->version >= FMOD_FEV_VERSION_46_0) {
        uint32_t sound_def_defs;

        sound_def_defs = read_fev_u32(fev, sf);
        for (int i = 0; i < sound_def_defs; i++)
            parse_fev_sound_def_def(fev, sf);
    }

    // finally at the sound defs which contain stream names;
    // the same stream entry can exist in multiple sounddefs
    // but only look for the 1st occurrence, unless you want
    // something along the lines of multiple cue names each.
    sound_defs = read_fev_u32(fev, sf);
    for (int i = 0; i < sound_defs; i++) {
        uint32_t offset, entries, entry_type, stream_index;

        if (fev->version >= FMOD_FEV_VERSION_65_0)
            fev->offset += 0x04; // name index?
        else if (!read_fev_string(fev, sf)) // sound def name
            return false;

        // 0x00: (v0x2E+) def index
        // 0x04: stream entries
        if (fev->version >= FMOD_FEV_VERSION_46_0)
            fev->offset += 0x04;
        else
            parse_fev_sound_def_def(fev, sf);

        entries = read_fev_u32(fev, sf);
        for (int j = 0; j < entries; j++) {
            // 0x00: entry type
            // 0x04: (v0x0E+) weight
            entry_type = read_fev_u32(fev, sf);
            if (fev->version >= FMOD_FEV_VERSION_14_0)
                fev->offset += 0x04;

            // enum: 0 = wavetable, 1 = oscillator, 2 = null, 3 = programmer
            if (entry_type == 0x00) {
                offset = fev->offset; // save pos

                if (!read_fev_string(fev, sf)) // stream name
                    return false;
                // TODO: compare and match against fsb filename
                if (fev->version >= FMOD_FEV_VERSION_65_0)
                    fev->offset += 0x04; // name index?
                else if (!read_fev_string(fev, sf)) // bank name
                    return false;

                // 0x00: stream index
                // 0x04: (v0x08+) length in ms
                stream_index = read_fev_u32(fev, sf);
                if (stream_index == target_subsong) {
                    fev->name_size = read_u32le(offset, sf);
                    fev->name_offset = offset + 0x04;
                    // found it, stop parsing
                    return true;
                }
                if (fev->version >= FMOD_FEV_VERSION_8_0)
                    fev->offset += 0x04;
            }
            else if (entry_type == 0x01) {
                // 0x00: oscillator type
                // 0x04: frequency
                fev->offset += 0x08;
            }
            else if (entry_type > 0x03)
                return false;
        }
    }

    // didn't find it?
    return false;
}

//static STREAMFILE* open_fev_filename_pair(STREAMFILE* sf_fsb) {}

#endif