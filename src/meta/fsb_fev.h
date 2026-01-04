#ifndef _FSB_FEV_H_
#define _FSB_FEV_H_
#include "../util/companion_files.h"


typedef struct {
    uint32_t version;

    off_t name_offset;
    size_t name_size;

    /* internal */
    off_t offset;
} fev_header_t;


// TODO: find real version examples from games
#define FMOD_FEV_VERSION_7_0    0x00070000  // MotoGP '06 (X360)
#define FMOD_FEV_VERSION_8_0    0x00080000  // MotoGP '06 (X360)
#define FMOD_FEV_VERSION_9_0    0x00090000  // ?
#define FMOD_FEV_VERSION_10_0   0x000A0000  // ?
#define FMOD_FEV_VERSION_11_0   0x000B0000  // ?
#define FMOD_FEV_VERSION_12_0   0x000C0000  // ?
#define FMOD_FEV_VERSION_13_0   0x000D0000  // ?
#define FMOD_FEV_VERSION_14_0   0x000E0000  // ?
//#define FMOD_FEV_VERSION_15_0 0x000F0000  // ?
#define FMOD_FEV_VERSION_16_0   0x00100000  // ?
//#define FMOD_FEV_VERSION_17_0 0x00110000  // ?
#define FMOD_FEV_VERSION_18_0   0x00120000  // ?
#define FMOD_FEV_VERSION_19_0   0x00130000  // ?
#define FMOD_FEV_VERSION_20_0   0x00140000  // ?
//#define FMOD_FEV_VERSION_21_0 0x00150000  // ?
#define FMOD_FEV_VERSION_22_0   0x00160000  // ?
#define FMOD_FEV_VERSION_23_0   0x00170000  // ?
#define FMOD_FEV_VERSION_24_0   0x00180000  // ?
#define FMOD_FEV_VERSION_25_0   0x00190000  // ?
#define FMOD_FEV_VERSION_26_0   0x001A0000  // ?
#define FMOD_FEV_VERSION_27_0   0x001B0000  // Paragraph 78 (PC)
#define FMOD_FEV_VERSION_28_0   0x001C0000  // ?
#define FMOD_FEV_VERSION_29_0   0x001D0000  // ?
#define FMOD_FEV_VERSION_30_0   0x001E0000  // ?
#define FMOD_FEV_VERSION_31_0   0x001F0000  // ?
#define FMOD_FEV_VERSION_32_0   0x00200000  // ?
//#define FMOD_FEV_VERSION_33_0 0x00210000  // ?
#define FMOD_FEV_VERSION_34_0   0x00220000  // Ys Online: The Call of Solum (PC)
//#define FMOD_FEV_VERSION_35_0 0x00230000  // ?
#define FMOD_FEV_VERSION_36_0   0x00240000  // ?
#define FMOD_FEV_VERSION_37_0   0x00250000  // Conan (X360)
#define FMOD_FEV_VERSION_38_0   0x00260000  // ?
#define FMOD_FEV_VERSION_39_0   0x00270000  // ?
#define FMOD_FEV_VERSION_40_0   0x00280000  // ?
#define FMOD_FEV_VERSION_41_0   0x00290000  // ?
#define FMOD_FEV_VERSION_42_0   0x002A0000  // ?
#define FMOD_FEV_VERSION_43_0   0x002B0000  // ?
#define FMOD_FEV_VERSION_44_0   0x002C0000  // Monster Jam (PS2)
#define FMOD_FEV_VERSION_45_0   0x002D0000  // ?
#define FMOD_FEV_VERSION_46_0   0x002E0000  // ?
//#define FMOD_FEV_VERSION_47_0 0x002F0000  // ?
//#define FMOD_FEV_VERSION_48_0 0x00300000  // ?
#define FMOD_FEV_VERSION_49_0   0x00310000  // ?
#define FMOD_FEV_VERSION_50_0   0x00320000  // Monster Jam: Urban Assault (PS2), Destroy All Humans: Path of the Furon (X360)
//#define FMOD_FEV_VERSION_51_0 0x00330000  // ?
#define FMOD_FEV_VERSION_52_0   0x00340000  // Stoked (X360), Indianapolis 500 Evolution (X360)
//#define FMOD_FEV_VERSION_53_0 0x00350000  // Bolt (PC/X360)
//#define FMOD_FEV_VERSION_54_0 0x00360000  // ?
//#define FMOD_FEV_VERSION_55_0 0x00370000  // AirRider CrazyRacing (PC)
#define FMOD_FEV_VERSION_56_0   0x00380000  // ?
#define FMOD_FEV_VERSION_57_0   0x00390000  // Birthday Party Bash (Wii)
#define FMOD_FEV_VERSION_58_0   0x003A0000  // Split/Second (PS3 QA Beta), Silent Hill: Shattered Memories (PS2), Rocket Knight (PS3)
//#define FMOD_FEV_VERSION_59_0 0x003B0000  // Just Cause 2 (PC), Renegade Ops (PS3)
#define FMOD_FEV_VERSION_60_0   0x003C0000  // ?
#define FMOD_FEV_VERSION_61_0   0x003D0000  // Split/Second (PS3/X360/PC), Armored Core V (PS3), NFS Shift (PS3), Supreme Commander 2 (PC)
#define FMOD_FEV_VERSION_62_0   0x003E0000  // Shank (PC), Stacking (X360)
#define FMOD_FEV_VERSION_63_0   0x003F0000  // ?
#define FMOD_FEV_VERSION_64_0   0x00400000  // Brutal Legend (PC), UFC Personal Trainer: The Ultimate Fitness (X360)
#define FMOD_FEV_VERSION_65_0   0x00410000  // ?
//#define FMOD_FEV_VERSION_66_0 0x00420000  // ?
//#define FMOD_FEV_VERSION_67_0 0x00430000  // ?
#define FMOD_FEV_VERSION_68_0   0x00440000  // ?
#define FMOD_FEV_VERSION_69_0   0x00450000  // ?


static inline uint32_t tell_fev(fev_header_t* fev) {
    return fev->offset;
}

static inline void seek_fev(fev_header_t* fev, uint32_t bytes) {
    // compilers can get a bit funny when doing multiple read
    // operations directly on top of a plain "fev->offset += "
    // so this forces it to pre-calc everything before adding
    fev->offset += bytes;
}

static inline uint32_t read_fev_u32le(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t ret = read_u32le(fev->offset, sf);
    fev->offset += 0x04;
    return ret;
}

static inline uint16_t read_fev_u16le(fev_header_t* fev, STREAMFILE* sf) {
    uint16_t ret = read_u16le(fev->offset, sf);
    fev->offset += 0x02;
    return ret;
}

static bool read_fev_string(char* buf, size_t buf_size, fev_header_t* fev, STREAMFILE* sf) {
    // strings are probably the only "sane" thing in FEVs, so enforce stricter checks
    uint32_t str_size;
    int i;

    // string size includes null terminator
    str_size = read_fev_u32le(fev, sf);
    if (str_size > buf_size) return false;
    if (str_size == 0x00) return true;

    for (i = 0; i < str_size - 1; i++) {
        // ASCII only for now, less lenient than read_string
        uint8_t c = read_u8(fev->offset++, sf);
        if (c < 0x20 || c > 0x7E) return false;
        if (buf) buf[i] = c;
    }
    if (read_u8(fev->offset++, sf) != 0x00)
        return false;
    if (buf) buf[i] = '\0';

    return true;
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

    properties = read_fev_u32le(fev, sf);
    for (int i = 0; i < properties; i++) {
        if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // property name
            return false;

        // enum: 0 = int, 1 = float, 2 = string
        property_type = read_fev_u32le(fev, sf);
        if (property_type < 0x00 || property_type > 0x02)
            return false;

        if (property_type != 0x02)
            seek_fev(fev, 0x04);
        else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // property
            return false;
    }

    return true;
}

static bool parse_fev_category(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t sub_categories;

    if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // category name
        return false;
    // 0x00: volume
    // 0x04: pitch
    // 0x08: (v0x29+) max playbacks
    // 0x0C: (v0x29+) max playbacks flags
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_41_0)
        seek_fev(fev, 0x08);

    sub_categories = read_fev_u32le(fev, sf);
    for (int i = 0; i < sub_categories; i++) {
        if (!parse_fev_category(fev, sf))
            return false;
    }

    return true;
}

static bool parse_fev_event_sound(fev_header_t* fev, STREAMFILE* sf) {

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        seek_fev(fev, 0x02); // name index?
    else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // sound name
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
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_30_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_31_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_36_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x04);
    if (fev->version <  FMOD_FEV_VERSION_52_0)
        seek_fev(fev, 0x08);
    seek_fev(fev, 0x0C);
    if (fev->version >= FMOD_FEV_VERSION_24_0)
        seek_fev(fev, 0x08);

    return true;
}

static bool parse_fev_event_envelope(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t points;

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        seek_fev(fev, 0x04); // parent envelope name index?
    else {
        if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // envelope name
            return false;
        if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // parent envelope name
            return false;
    }

    if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // dsp unit name
        return false;

    // 0x00: dsp param index
    // 0x04: (v0x26+) flags
    // 0x08: (v0x39+) more flags
    // 0x0C: points[]
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_38_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_57_0)
        seek_fev(fev, 0x04);
    points = read_fev_u32le(fev, sf);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        seek_fev(fev, points * 0x04); // idx
    else {
        seek_fev(fev, points * 0x08); // x,y
        if (fev->version >= FMOD_FEV_VERSION_13_0)
            seek_fev(fev, points * 0x04); // flags
    }
    // params/flags
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_26_0)
        seek_fev(fev, 0x04);

    return true;
}

static bool parse_fev_event_complex(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t layers, params, sounds, envelopes;

    layers = read_fev_u32le(fev, sf);
    for (int i = 0; i < layers; i++) {
        if (fev->version <  FMOD_FEV_VERSION_39_0) {
            if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // layer name
                return false;
        }

        // 0x00: uses software (u32), (v0x1D+) flags (u16)
        // 0x02: (v0x25+) priority (u16)
        // 0x06: param name (str), (v0x27+) param index (u16)
        // 0x08: sounds (u32), (v0x1D+) sounds (u16)
        // 0x0A: envelopes (u32), (v0x1D+) envelopes (u16)
        fev->version >= FMOD_FEV_VERSION_29_0
            ? seek_fev(fev, 0x02)
            : seek_fev(fev, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_37_0)
            seek_fev(fev, 0x02);
        if (fev->version >= FMOD_FEV_VERSION_39_0)
            seek_fev(fev, 0x02);
        else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // param name
            return false;
        if (fev->version >= FMOD_FEV_VERSION_29_0) {
            sounds    = read_fev_u16le(fev, sf);
            envelopes = read_fev_u16le(fev, sf);
        }
        else {
            sounds    = read_fev_u32le(fev, sf);
            envelopes = read_fev_u32le(fev, sf);
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

    params = read_fev_u32le(fev, sf);
    for (int i = 0; i < params; i++) {
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            seek_fev(fev, 0x04); // name index?
        else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // param name
            return false;

        // 0x00: (<v0x12) min?
        // 0x04: (<v0x12) max?
        // 0x08: velocity
        // 0x0C: range min original
        // 0x10: range max original
        // 0x14: primary(?), (v0x10+) flags
        // 0x18: (v0x0B~v0x10) loop mode
        // 0x1C: (v0x12+) seek speed
        // 0x20: envelopes
        // 0x24: (v0x0C+) sustain points[]
        if (fev->version <  FMOD_FEV_VERSION_18_0)
            seek_fev(fev, 0x08);
        seek_fev(fev, 0x10);
        if (fev->version >= FMOD_FEV_VERSION_11_0 &&
            fev->version <  FMOD_FEV_VERSION_16_0)
            seek_fev(fev, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_18_0)
            seek_fev(fev, 0x04);
        seek_fev(fev, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_12_0)
            seek_fev(fev, read_fev_u32le(fev, sf) * 0x04);
    }

    if (!parse_fev_properties(fev, sf))
        return false;

    return true;
}

static bool parse_fev_event_simple(fev_header_t* fev, STREAMFILE* sf) {

    seek_fev(fev, 0x04); // flags
    if (!parse_fev_event_sound(fev, sf))
        return false;

    return true;
}

static bool parse_fev_event(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t event_type, categories;

    event_type = 0x08; // 0x08 = complex, 0x10 = simple
    if (fev->version >= FMOD_FEV_VERSION_52_0)
        event_type = read_fev_u32le(fev, sf);

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        seek_fev(fev, 0x04); // name index?
    else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // event name
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
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_32_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_10_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_56_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x0C);
    if (fev->version >= FMOD_FEV_VERSION_69_0)
        seek_fev(fev, 0x08);
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_9_0)
        seek_fev(fev, 0x2C);
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_28_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_18_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_19_0)
        seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_43_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_45_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_22_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_40_0)
        seek_fev(fev, 0x04);

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

    categories = read_fev_u32le(fev, sf);
    for (int i = 0; i < categories; i++) {
        if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf))
            return false;
    }

    return true;
}

static bool parse_fev_event_category(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t sub_groups, events;

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        seek_fev(fev, 0x04); // name index?
    else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // event category name
        return false;

    if (fev->version >= FMOD_FEV_VERSION_23_0) {
        if (!parse_fev_properties(fev, sf))
            return false;
    }

    sub_groups = read_fev_u32le(fev, sf);
    events     = read_fev_u32le(fev, sf);

    for (int i = 0; i < sub_groups; i++) {
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
    // 0x04: spawn time min, (v0x22~v0x26) spawn intensity
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
    // 0x44: (v0x3E+) trigger delay min (u16)
    // 0x46: (v0x3E+) trigger delay max (u16)
    // 0x48: (v0x3F+) spawn count (u16)
    seek_fev(fev, 0x08);
    if (fev->version <  FMOD_FEV_VERSION_34_0 ||
        fev->version >= FMOD_FEV_VERSION_38_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        seek_fev(fev, 0x04);
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_60_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_42_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_62_0)
        seek_fev(fev, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_63_0)
        seek_fev(fev, 0x02);

}


static bool parse_fev(fev_header_t* fev, STREAMFILE* sf, char* fsb_wavebank_name) {
    // initial research based on this which appears to be for FEV v0x34~v0x38
    // https://github.com/xoreos/xoreos/blob/master/src/sound/fmodeventfile.cpp
    // further research from FMOD::EventSystemI::load in Split/Second's fmod_event.dll
    // lastly also found this project by putting fmod_event.dll's func name in github search
    // https://github.com/barspinoff/bmod/blob/main/tools/fmod_event/src/fmod_eventsystemi.cpp
    uint32_t wave_banks, event_groups, sound_defs, languages = 1;
    int target_subsong = sf->stream_index, bank_name_idx = -1;
    char wavebank_name[STREAM_NAME_SIZE];

    if (!is_id32be(0x00, sf, "FEV1"))
        return false;

    fev->version = read_u32le(0x04, sf);
    // fmod_event.dll rejects any version below 7.0
    // last is v69.0 but do those FEV1s even exist?
    // (0x41+ might be only found in FSB5/RIFF FEV)
    if ((fev->version & 0xFF00FFFF) ||
        fev->version < FMOD_FEV_VERSION_7_0 ||
        fev->version > FMOD_FEV_VERSION_69_0)
        return false;

    if (target_subsong == 0) target_subsong = 1;
    target_subsong--;

    // by far the biggest issue with FEV is the lack of pointers to anything,
    // so everything has to be read in sequence until you get to stream names

    // 0x00: FEV1
    // 0x04: version
    // 0x08: (v0x2E+) sound def pool
    // 0x0C: (v0x32+) 64-bit sound def pool
    // 0x10: (v0x40+) object table[] {idx, alloc_size}
    seek_fev(fev, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_46_0)
        seek_fev(fev, 0x04); // fmod_event.dll v0x3E still reads this
    if (fev->version >= FMOD_FEV_VERSION_50_0)
        seek_fev(fev, 0x04); // fmod_event.dll v0x3E seeks over this
    if (fev->version >= FMOD_FEV_VERSION_64_0)
        seek_fev(fev, read_fev_u32le(fev, sf) * 0x08);

    // FEV bank name (should match filename)
    if (fev->version >= FMOD_FEV_VERSION_25_0) {
        if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf))
            return false;
    }


    // 0x00: sound banks
    // 0x04: (v0x41+) languages (32 max, not enforced?)
    wave_banks = read_fev_u32le(fev, sf);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        languages = read_fev_u32le(fev, sf);

    for (int i = 0; i < wave_banks; i++) {
        // 0x00: mode/flags?
        // 0x04: (v0x14+) max streams
        // 0x08: (v0x3D+) hash[]
        // 0x0C: (v0x41+) fsb suffix[](?)
        // 0x10: FSB bank name (should match filename)
        seek_fev(fev, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_20_0)
            seek_fev(fev, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_61_0)
            seek_fev(fev, 0x08 * languages);
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            seek_fev(fev, 0x04 * languages);

        if (!read_fev_string(wavebank_name, STREAM_NAME_SIZE, fev, sf)) // wave bank name
            return false;
        // v0x41+ just store indices later, no samples to go
        // with yet, but assuming they're referring to these
        if (fev->version >= FMOD_FEV_VERSION_65_0) {
            if (strncasecmp(wavebank_name, fsb_wavebank_name, STREAM_NAME_SIZE) == 0) {
                if (bank_name_idx != -1)
                    vgm_logi("FEV: Multiple identical bank names!\n");
                bank_name_idx = i;
            }
        }
    }

    if (!parse_fev_category(fev, sf))
        return false;

    event_groups = read_fev_u32le(fev, sf);
    for (int i = 0; i < event_groups; i++) {
        if (!parse_fev_event_category(fev, sf))
            return false;
    }

    // finally at the sound defs which contain stream names;
    // the same stream entry can exist in multiple sounddefs
    // but only look for the 1st occurrence, unless you want
    // something along the lines of multiple cue names each.
    if (fev->version >= FMOD_FEV_VERSION_46_0) {
        uint32_t sound_def_defs;

        sound_def_defs = read_fev_u32le(fev, sf);
        for (int i = 0; i < sound_def_defs; i++)
            parse_fev_sound_def_def(fev, sf);
    }

    sound_defs = read_fev_u32le(fev, sf);
    for (int i = 0; i < sound_defs; i++) {
        uint32_t entries, entry_type, stream_index;

        // this could be used as a cue name
        // a stream can be in multiple cues
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            seek_fev(fev, 0x04); // name index?
        else if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // sound def name
            return false;

        // 0x00: (v0x2E+) def index
        // 0x04: stream entries
        if (fev->version >= FMOD_FEV_VERSION_46_0)
            seek_fev(fev, 0x04);
        else
            parse_fev_sound_def_def(fev, sf);

        entries = read_fev_u32le(fev, sf);
        for (int j = 0; j < entries; j++) {
            // 0x00: entry type
            // 0x04: (v0x0E+) weight
            entry_type = read_fev_u32le(fev, sf);
            if (fev->version >= FMOD_FEV_VERSION_14_0)
                seek_fev(fev, 0x04);

            // enum: 0 = wavetable, 1 = oscillator, 2 = null, 3 = programmer
            if (entry_type == 0x00) {
                uint32_t stream_name_offset;
                bool is_target_bank = false;

                stream_name_offset = tell_fev(fev);
                if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf)) // stream name
                    return false;

                if (fev->version >= FMOD_FEV_VERSION_65_0) {
                    // as above, no actual v0x41+ samples to confirm, but...
                    is_target_bank = (read_fev_u32le(fev, sf) == bank_name_idx);
                }
                else {
                    if (!read_fev_string(wavebank_name, STREAM_NAME_SIZE, fev, sf)) // bank name
                        return false;
                    // case should match, but some games have all-upper or all-lower filenames
                    if (strncasecmp(wavebank_name, fsb_wavebank_name, STREAM_NAME_SIZE) == 0)
                        is_target_bank = true;
                }

                // 0x00: stream index
                // 0x04: (v0x08+) length in ms
                stream_index = read_fev_u32le(fev, sf);
                if (is_target_bank && stream_index == target_subsong) {
                    fev->name_size = read_u32le(stream_name_offset, sf);
                    fev->name_offset = stream_name_offset + 0x04;
                    // found it, stop parsing
                    return true;
                }
                if (fev->version >= FMOD_FEV_VERSION_8_0)
                    seek_fev(fev, 0x04);
            }
            else if (entry_type == 0x01) {
                // 0x00: oscillator type
                // 0x04: frequency
                seek_fev(fev, 0x08);
            }
            else if (entry_type > 0x03)
                return false;
        }
    }

    // anything beyond this is not relevant for song names
    // but the implementation is here for future reference
    // (although rarely seen used, the comp cues chunk may
    // also be of some interest to append to stream names)
#if 0
    if (fev->version >= FMOD_FEV_VERSION_21_0) {
        uint32_t reverb_defs;

        reverb_defs = read_fev_u32le(fev, sf);
        for (int i = 0; i < reverb_defs; i++) {
            if (!read_fev_string(NULL, STREAM_NAME_SIZE, fev, sf))
                return false;

            // 0x00: room
            // 0x04: room HF
            // 0x08: DEPRECATED
            // 0x0C: decay time
            // 0x10: decay HF ratio
            // 0x14: reflections
            // 0x18: reflections delay
            // 0x1C: reverb
            // 0x20: reverb delay
            // 0x24: diffusion
            // 0x28: density
            // 0x2C: HF reference
            // 0x30: (v0x1C+) room LF
            // 0x34: (v0x1C+) LF reference
            // 0x38: instance
            // 0x3C: environment
            // 0x40: DEPRECATED env size
            // 0x44: env diffusion
            // 0x48: room LF
            // 0x4C: decay LF ratio
            // 0x50: DEPRECATED reflections pan[3]
            // 0x5C: DEPRECATED reverb pan[3]
            // 0x68: DEPRECATED echo time
            // 0x6C: DEPRECATED echo depth
            // 0x70: modulation time
            // 0x74: modulation depth
            // 0x78: DEPRECATED air absorption HF
            // 0x7C: LF reference
            // 0x80: flags
            seek_fev(fev, 0x30);
            if (fev->version >= FMOD_FEV_VERSION_28_0)
                seek_fev(fev, 0x08);
            seek_fev(fev, 0x4C);
        }
    }

    // composition data, similar to RIFF layout
    // [Critter Crunch (PS3), Bolt (PC/X360)]
    if (fev->version >= FMOD_FEV_VERSION_47_0) {
        uint32_t comp_end, chunk_size, chunk_id;
        comp_end = get_streamfile_size(sf);

        do {
            chunk_size = read_fev_u32le(fev, sf);
            // LE in v0x2F, BE in v0x30+ (official docs say the opposite)
            chunk_id = (fev->version == FMOD_FEV_VERSION_47_0)
                ? read_fev_u32le(fev, sf)
                : read_fev_u32be(fev, sf);

            switch (chunk_size) {
                case 0x636F6D70: // "comp" composition container header
                    comp_end = tell_fev(fev) - 0x08 + chunk_size;
                    break;

                case 0x73657474: // "sett" music settings
                    // 0x00: volume
                    // 0x04: reverb
                    seek_fev(fev, 0x08);
                    break;

                case 0x6C6E6B73: // "lnks" link container
                    // sub-chunks: "lnkh", "lnk ", "lnkd", "cond", "cprm", "cms ", "cc  ", "lfsh", "lfsd"
                case 0x63756573: // "cues" cue container
                    // sub-chunks: "entl"
                case 0x70726D73: // "prms" parameter container
                    // sub-chunks: "prmh", "prmd", "entl"
                case 0x73636E73: // "scns" scene container
                    // sub-chunks: "scnh", "scnd"
                case 0x74686D73: // "thms" theme container
                    // sub-chunks: "thmh", "thm ", "thmd", "cond", "cprm", "cms ", "cc  "
                case 0x746C6E73: // "tlns" timeline container
                    // sub-chunks: "tlnh", "tlnd"
                case 0x73676D73: // "sgms" segment container
                    // sub-chunks: "sgmh", "sgmd", "smpf", "str ", "smpm"
                default: // unknown
                    seek_fev(fev, chunk_size - 0x08);
                    break;
            }
        } while (tell_fev(fev) < comp_end);
        // should just be until EOF
    }
#endif

    // can rarely have no name in both FSB/FEV [Stoked (X360)]
    return true;
}


static STREAMFILE* open_fev_filename_pair(STREAMFILE* sf_fsb) {
    STREAMFILE* sf_fev = NULL;

    // try parsing TXTM if present
    sf_fev = read_filemap_file(sf_fsb, 0);
    if (sf_fev) return sf_fev;

    sf_fev = open_streamfile_by_ext(sf_fsb, "fev");
    if (sf_fev) return sf_fev;

    return NULL;
}

#endif
