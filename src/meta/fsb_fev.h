#ifndef _FSB_FEV_H_
#define _FSB_FEV_H_
#include "../util/companion_files.h"
#include "../util/reader_helper.h"
#include "../util/chunks.h"


typedef struct {
    /* input */
    int target_subsong; // zero indexed
    char fsb_wavebank_name[STREAM_NAME_SIZE];

    /* state */
    uint32_t version;
    int bank_name_idx;
    uint32_t languages;
    // comp>sgms>smpf info
    uint32_t comp_strings;
    off_t comp_string_ofs;
    off_t comp_string_buf;
    uint32_t comp_samples;
    off_t comp_sample_ofs;
    // RIFF>LIST>STRR info
    uint32_t riff_strings;
    off_t riff_string_ofs;
    off_t riff_string_buf;

    /* output */
    char stream_name[STREAM_NAME_SIZE];
    size_t stream_name_size;
    /*
    char sound_name[STREAM_NAME_SIZE];
    size_t sound_name_len;
    char sound_def_name[STREAM_NAME_SIZE];
    size_t sound_def_name_len;
    // merged output of the two strings above
    char output_stream_name[STREAM_NAME_SIZE];
    */
} fev_header_t;


// TODO: find real version examples from games
#define FMOD_FEV_VERSION_7_0    0x00070000  // MotoGP '06 (X360, Ambient)
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
#define FMOD_FEV_VERSION_21_0   0x00150000  // ?
#define FMOD_FEV_VERSION_22_0   0x00160000  // ?
#define FMOD_FEV_VERSION_23_0   0x00170000  // ?
#define FMOD_FEV_VERSION_24_0   0x00180000  // ?
#define FMOD_FEV_VERSION_25_0   0x00190000  // ?
#define FMOD_FEV_VERSION_26_0   0x001A0000  // ?
#define FMOD_FEV_VERSION_27_0   0x001B0000  // Paragraph 78 (PC), Ratatouille (PS3, MG1Ds;MG1Cs)
#define FMOD_FEV_VERSION_28_0   0x001C0000  // ?
#define FMOD_FEV_VERSION_29_0   0x001D0000  // ?
#define FMOD_FEV_VERSION_30_0   0x001E0000  // ?
#define FMOD_FEV_VERSION_31_0   0x001F0000  // ?
#define FMOD_FEV_VERSION_32_0   0x00200000  // ?
//#define FMOD_FEV_VERSION_33_0 0x00210000  // Spider-Man 3 (PS2)
#define FMOD_FEV_VERSION_34_0   0x00220000  // Ys Online: The Call of Solum (PC), Ratatouille (PS3)
//#define FMOD_FEV_VERSION_35_0 0x00230000  // ?
#define FMOD_FEV_VERSION_36_0   0x00240000  // ?
#define FMOD_FEV_VERSION_37_0   0x00250000  // Conan (X360), Manhunt 2 (Wii), Wall-E (PS3, WT01s;WT02s;WT40s)
#define FMOD_FEV_VERSION_38_0   0x00260000  // SpongeBob: Attack of the Toybots (PS2) WWE SmackDown vs. Raw 2008 (PS3)
#define FMOD_FEV_VERSION_39_0   0x00270000  // ?
#define FMOD_FEV_VERSION_40_0   0x00280000  // ?
#define FMOD_FEV_VERSION_41_0   0x00290000  // ?
#define FMOD_FEV_VERSION_42_0   0x002A0000  // Kung Fu Panda (PS3)
#define FMOD_FEV_VERSION_43_0   0x002B0000  // Wall-E (PS3, ED01s;FANBs)
#define FMOD_FEV_VERSION_44_0   0x002C0000  // Monster Jam (PS2), Manhunt 2 (PC)
#define FMOD_FEV_VERSION_45_0   0x002D0000  // Iron Man (PS3)
#define FMOD_FEV_VERSION_46_0   0x002E0000  // ?
#define FMOD_FEV_VERSION_47_0   0x002F0000  // ?
#define FMOD_FEV_VERSION_48_0   0x00300000  // ?
#define FMOD_FEV_VERSION_49_0   0x00310000  // ?
#define FMOD_FEV_VERSION_50_0   0x00320000  // Monster Jam: Urban Assault (PS2/PSP), Destroy All Humans! Path of the Furon (X360), Wall-E (PS3)
#define FMOD_FEV_VERSION_51_0   0x00330000  // ?
#define FMOD_FEV_VERSION_52_0   0x00340000  // Stoked (X360), Indianapolis 500 Evolution (X360)
#define FMOD_FEV_VERSION_53_0   0x00350000  // Bolt (PC/X360), LittleBigPlanet 1/2/3 (PS3), SpongeBob: Globs of Doom (PS2)
//#define FMOD_FEV_VERSION_54_0 0x00360000  // The Fight (PS3, BoxoMusic), Move Fitness (PS3, BoxoMusic), Bolt (PS2)
//#define FMOD_FEV_VERSION_55_0 0x00370000  // AirRider CrazyRacing (PC), Cars: Race-O-Rama (PSP), MX vs. ATV Reflex (PSP), Coraline (PS2)
#define FMOD_FEV_VERSION_56_0   0x00380000  // Assassin's Creed: Bloodlines (PSP), X-Men Origins: Wolverine (PS2/PSP)
#define FMOD_FEV_VERSION_57_0   0x00390000  // Birthday Party Bash (Wii)
#define FMOD_FEV_VERSION_58_0   0x003A0000  // Silent Hill: Shattered Memories (PS2/PSP), Rocket Knight (PS3), G.I. Joe: The Rise of Cobra (PS2/PSP)
//#define FMOD_FEV_VERSION_59_0 0x003B0000  // Just Cause 2 (PC), Renegade Ops (PS3)
#define FMOD_FEV_VERSION_60_0   0x003C0000  // ?
#define FMOD_FEV_VERSION_61_0   0x003D0000  // Split/Second (PS3/X360/PC), Armored Core V (PS3), NFS Shift (PS3), Supreme Commander 2 (PC)
#define FMOD_FEV_VERSION_62_0   0x003E0000  // Shank (PS3/X360/PC), Stacking (X360), Kung Fu Panda 2 (PS3), Ben 10: Cosmic Destruction (PS3/PSP)
#define FMOD_FEV_VERSION_63_0   0x003F0000  // ?
#define FMOD_FEV_VERSION_64_0   0x00400000  // Brutal Legend (PC), UFC Personal Trainer (X360), Rugby Challenge (PS3), NFS Shift 2 (PS3)
#define FMOD_FEV_VERSION_65_0   0x00410000  // ?
//#define FMOD_FEV_VERSION_66_0 0x00420000  // ?
//#define FMOD_FEV_VERSION_67_0 0x00430000  // Green Lantern: Rise of the Manhunters (PS3), Orcs Must Die! 2 (PC), Move Fitness (PS3)
#define FMOD_FEV_VERSION_68_0   0x00440000  // ?
#define FMOD_FEV_VERSION_69_0   0x00450000  // Cars 2 (PSP), Brave (PS3), Badland (PSV), Dark Souls III (PC), Stronghold: Warlords (PC)


static size_t append_fev_string(char* buf, size_t buf_size, char* str, size_t str_size) {
    if (!str_size) // append new string
        return snprintf(buf, buf_size, "%s", str);
    else if (str_size < buf_size) // append secondary string
        return snprintf(&buf[str_size], buf_size - str_size, "; %s", str);
    return 0; // didn't append anything, buf_size limit reached
}


// LE in v0x2F, BE in v0x30+ (official docs say the opposite)
static inline uint32_t reader_fev_chunk_id(fev_header_t* fev, reader_t* r) {
    uint32_t id = reader_u32(r);
    if (fev->version >= FMOD_FEV_VERSION_48_0)
        id = id >> 24 | (id >> 8 & 0xFF00) | (id & 0xFF00) << 8 | (id & 0xFF) << 24;
    return id;
}

// strings are probably the only "sane" thing in FEVs, so enforce stricter checks
static bool reader_fev_string(char* buf, size_t buf_size, reader_t* r) {
    uint32_t str_size, read_size;

    // string size includes null terminator
    str_size = reader_u32(r);
    if (str_size > buf_size) return false;
    if (!str_size) {
        if (buf) buf[0] = '\0';
        return true;
    }

    read_size = read_string(buf, buf_size, r->offset, r->sf);
    if (read_size != str_size - 1) return false;

    r->offset += str_size;
    return true;
}

// bank names rarely include a directory name to be stripped [Manhunt 2 (Wii/PC)]
static bool reader_fev_bankname(char* buf, size_t buf_size, reader_t* r) {
    char full[STREAM_NAME_SIZE];
    char* base;

    if (!reader_fev_string(full, STREAM_NAME_SIZE, r))
        return false;

    // TODO: posix path separator only(?)
    base = strrchr(full, '/');
    snprintf(buf, buf_size, "%s", base ? base + 1 : full);

    return true;
}

// similarly to strings, UUIDs are one of the rare "sane" things to check for
static bool reader_fev_uuid(reader_t* r) {
    uint16_t uuid_seg3, uuid_seg4;

    uuid_seg3 = read_u16le(r->offset + 0x06, r->sf);
    uuid_seg4 = read_u16be(r->offset + 0x08, r->sf);

    if ((uuid_seg3 & 0xF000) != 0x4000 ||
        (uuid_seg4 & 0xF000) <  0x8000 ||
        (uuid_seg4 & 0xF000) >  0xB000)
        return false;

    r->offset += 0x10;
    return true;
}


static bool parse_fev_properties(fev_header_t* fev, reader_t* r) {
    uint32_t properties, property_type;

    properties = reader_u32(r);
    for (int i = 0; i < properties; i++) {
        if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // property name
            return false;

        // enum: 0 = int, 1 = float, 2 = string
        property_type = reader_u32(r);
        if (property_type < 0x00 || property_type > 0x02)
            return false;

        if (property_type != 0x02)
            reader_skip(r, 0x04);
        else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // property
            return false;
    }

    return true;
}

static bool parse_fev_category(fev_header_t* fev, reader_t* r) {
    uint32_t sub_categories;

    if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // category name
        return false;
    // 0x00: volume
    // 0x04: pitch
    // 0x08: (v0x29+) max playbacks
    // 0x0C: (v0x29+) max playbacks flags
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_41_0)
        reader_skip(r, 0x08);

    sub_categories = reader_u32(r);
    for (int i = 0; i < sub_categories; i++) {
        if (!parse_fev_category(fev, r))
            return false;
    }

    return true;
}

static bool parse_fev_event_sound(fev_header_t* fev, reader_t* r) {

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        reader_skip(r, 0x02); // name index?
    else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // sound name
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
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_30_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_31_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_36_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x04);
    if (fev->version <  FMOD_FEV_VERSION_52_0)
        reader_skip(r, 0x08);
    reader_skip(r, 0x0C);
    if (fev->version >= FMOD_FEV_VERSION_24_0)
        reader_skip(r, 0x08);

    return true;
}

static bool parse_fev_event_envelope(fev_header_t* fev, reader_t* r) {
    uint32_t points;

    if (fev->version >= FMOD_FEV_VERSION_39_0)
        reader_skip(r, 0x04); // parent envelope name index?
    else {
        if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // envelope name
            return false;
        if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // parent envelope name
            return false;
    }

    if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // dsp unit name
        return false;

    // 0x00: dsp param index
    // 0x04: (v0x26+) flags
    // 0x08: (v0x39+) more flags
    // 0x0C: points[]
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_38_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_57_0)
        reader_skip(r, 0x04);
    points = reader_u32(r);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        reader_skip(r, points * 0x04); // idx
    else {
        reader_skip(r, points * 0x08); // x,y
        if (fev->version >= FMOD_FEV_VERSION_13_0)
            reader_skip(r, points * 0x04); // flags
    }
    // params/flags
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_26_0)
        reader_skip(r, 0x04);

    return true;
}

static bool parse_fev_event_complex(fev_header_t* fev, reader_t* r) {
    uint32_t layers, params, sounds, envelopes;

    layers = reader_u32(r);
    for (int i = 0; i < layers; i++) {
        if (fev->version <  FMOD_FEV_VERSION_39_0) {
            if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // layer name
                return false;
        }

        // 0x00: uses software (u32), (v0x1D+) flags (u16)
        // 0x02: (v0x25+) priority (u16)
        // 0x06: param name (str), (v0x27+) param index (u16)
        // 0x08: sounds (u32), (v0x1D+) sounds (u16)
        // 0x0A: envelopes (u32), (v0x1D+) envelopes (u16)
        reader_skip(r, 0x02);
        if (fev->version <  FMOD_FEV_VERSION_29_0)
            reader_skip(r, 0x02);
        if (fev->version >= FMOD_FEV_VERSION_37_0)
            reader_skip(r, 0x02);
        if (fev->version >= FMOD_FEV_VERSION_39_0)
            reader_skip(r, 0x02);
        else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // param name
            return false;
        if (fev->version >= FMOD_FEV_VERSION_29_0) {
            sounds    = reader_u16(r);
            envelopes = reader_u16(r);
        }
        else {
            sounds    = reader_u32(r);
            envelopes = reader_u32(r);
        }

        for (int j = 0; j < sounds; j++) {
            if (!parse_fev_event_sound(fev, r))
                return false;
        }
        for (int j = 0; j < envelopes; j++) {
            if (!parse_fev_event_envelope(fev, r))
                return false;
        }
    }

    params = reader_u32(r);
    for (int i = 0; i < params; i++) {
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            reader_skip(r, 0x04); // name index
        else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // param name
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
            reader_skip(r, 0x08);
        reader_skip(r, 0x10);
        if (fev->version >= FMOD_FEV_VERSION_11_0 &&
            fev->version <  FMOD_FEV_VERSION_16_0)
            reader_skip(r, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_18_0)
            reader_skip(r, 0x04);
        reader_skip(r, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_12_0)
            reader_skip(r, reader_u32(r) * 0x04);
    }

    if (!parse_fev_properties(fev, r))
        return false;

    return true;
}

static bool parse_fev_event_simple(fev_header_t* fev, reader_t* r) {

    reader_skip(r, 0x04); // flags
    if (!parse_fev_event_sound(fev, r))
        return false;

    return true;
}

static bool parse_fev_event(fev_header_t* fev, reader_t* r) {
    uint32_t event_type, categories;

    event_type = 0x08; // 0x08 = complex, 0x10 = simple
    if (fev->version >= FMOD_FEV_VERSION_52_0)
        event_type = reader_u32(r);

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        reader_skip(r, 0x04); // name index
    else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // event name
        return false;

    if (fev->version >= FMOD_FEV_VERSION_58_0) {
        if (!reader_fev_uuid(r))
            return false;
    }

    // 0x00: volume
    // 0x04: pitch
    // 0x08: (v0x1B+) pitch randomisation
    // 0x0C: (v0x20+) volume randomisation (inverted before v0x21)
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
    // 0x60: (v0x0B+) max playbacks flags (0 if >4 before v0x23)
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
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_32_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_10_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_56_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x0C);
    if (fev->version >= FMOD_FEV_VERSION_69_0)
        reader_skip(r, 0x08);
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_9_0)
        reader_skip(r, 0x2C);
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_28_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_11_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_18_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_19_0)
        reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_43_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_45_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_22_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_40_0)
        reader_skip(r, 0x04);

    if ((event_type & 0x18) == 0x08) {
        if (!parse_fev_event_complex(fev, r))
            return false;
    }
    else if ((event_type & 0x18) == 0x10) {
        if (!parse_fev_event_simple(fev, r))
            return false;
    }
    else {
        vgm_logi("FEV: Invalid event configuration\n");
        return false;
    }

    categories = reader_u32(r);
    for (int i = 0; i < categories; i++) {
        if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // category name
            return false;
    }

    return true;
}

static bool parse_fev_event_category(fev_header_t* fev, reader_t* r) {
    uint32_t sub_groups, events;

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        reader_skip(r, 0x04); // name index
    else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // event category name
        return false;

    if (fev->version >= FMOD_FEV_VERSION_23_0) {
        if (!parse_fev_properties(fev, r))
            return false;
    }

    sub_groups = reader_u32(r);
    events     = reader_u32(r);

    for (int i = 0; i < sub_groups; i++) {
        if (!parse_fev_event_category(fev, r))
            return false;
    }
    for (int i = 0; i < events; i++) {
        if (!parse_fev_event(fev, r))
            return false;
    }

    return true;
}

static bool parse_fev_reverb_def(fev_header_t* fev, reader_t* r) {

    if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // reverb def name
        return false;

    // 0x00: room
    // 0x04: room HF
    // 0x08: DEPRECATED(?)
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
    reader_skip(r, 0x30);
    if (fev->version >= FMOD_FEV_VERSION_28_0)
        reader_skip(r, 0x08);
    reader_skip(r, 0x4C);

    return true;
}


static void parse_fev_sound_def_def(fev_header_t* fev, reader_t* r) {

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
    reader_skip(r, 0x08);
    if (fev->version <  FMOD_FEV_VERSION_34_0 ||
        fev->version >= FMOD_FEV_VERSION_38_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        reader_skip(r, 0x04);
    reader_skip(r, 0x08);
    if (fev->version >= FMOD_FEV_VERSION_27_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_60_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_68_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_42_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_62_0)
        reader_skip(r, 0x04);
    if (fev->version >= FMOD_FEV_VERSION_63_0)
        reader_skip(r, 0x02);

}

static bool parse_fev_sound_def(fev_header_t* fev, reader_t* r) {
    uint32_t entries, entry_type, stream_index;

    if (fev->version >= FMOD_FEV_VERSION_65_0)
        reader_skip(r, 0x04); // name index
    else if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r)) // sound def name
        return false;

    // 0x00: (v0x2E+) def index
    // 0x04: stream entries
    if (fev->version >= FMOD_FEV_VERSION_46_0)
        reader_skip(r, 0x04);
    else
        parse_fev_sound_def_def(fev, r);

    entries = reader_u32(r);
    for (int i = 0; i < entries; i++) {
        // 0x00: entry type
        // 0x04: (v0x0E+) weight (actually used from v0x11+?)
        entry_type = reader_u32(r);
        if (fev->version >= FMOD_FEV_VERSION_14_0)
            reader_skip(r, 0x04);

        // enum: 0 = wavetable, 1 = oscillator, 2 = null, 3 = programmer
        switch (entry_type) {
            case 0x00: {
                char wavebank_name[STREAM_NAME_SIZE];
                char stream_name[STREAM_NAME_SIZE];
                bool is_target_bank = false;

                if (!reader_fev_string(stream_name, STREAM_NAME_SIZE, r)) // stream name
                    return false;

                if (fev->version >= FMOD_FEV_VERSION_65_0)
                    is_target_bank = (reader_u32(r) == fev->bank_name_idx);
                else {
                    if (!reader_fev_bankname(wavebank_name, STREAM_NAME_SIZE, r)) // bank name
                        return false;
                    // case should match, but some games have all-upper or all-lower filenames
                    if (strncasecmp(wavebank_name, fev->fsb_wavebank_name, STREAM_NAME_SIZE) == 0)
                        is_target_bank = true;
                }

                // 0x00: stream index
                // 0x04: (v0x08+) length in ms
                stream_index = reader_u32(r);
                if (fev->version >= FMOD_FEV_VERSION_8_0)
                    reader_skip(r, 0x04);

                // can have multiple names pointing to the same stream, FSB only stores one (usually first or last?)
                if (is_target_bank && stream_index == fev->target_subsong && !strstr(fev->stream_name, stream_name))
                    fev->stream_name_size += append_fev_string(fev->stream_name, STREAM_NAME_SIZE, stream_name, fev->stream_name_size);

                break;
            }

            case 0x01:
                // 0x00: oscillator type
                // 0x04: frequency
                reader_skip(r, 0x08);
                break;

            case 0x02:
            case 0x03:
                break;

            default:
                return false;
        }
    }

    //if (has_stream_name)
    //    fev->sound_def_name_len += append_fev_string(fev->sound_def_name, STREAM_NAME_SIZE, sound_def_name, fev->sound_def_name_len);

    return true;
}


static bool parse_fev_composition_segment_sample(fev_header_t* fev, reader_t* r, uint32_t target_segment_id, uint32_t target_sample_idx) {
    char wavebank_name[STREAM_NAME_SIZE];
    uint32_t stream_index;

    // using reader_t, still part of the "sgmd" chunk
    if (!reader_fev_bankname(wavebank_name, STREAM_NAME_SIZE, r))
        return false;
    stream_index = reader_u32(r);

    // using standard sf i/o to read from later chunks if it matches
    if (stream_index == fev->target_subsong && strncasecmp(wavebank_name, fev->fsb_wavebank_name, STREAM_NAME_SIZE) == 0) {
        // rare but multiple matches can occur, however their corresponding smpm data still points
        // to the same str chunk index [When Vikings Attack! (PSV) - portalboss_bank00.fsb#1,2,3,8]
        uint32_t segment_id, sample_idx, string_idx, name_offset;
        char stream_name[STREAM_NAME_SIZE];

        for (int i = 0; i < fev->comp_samples; i++) {
            segment_id = read_u32le(fev->comp_sample_ofs + i * 0x0C + 0x00, r->sf);
            sample_idx = read_u32le(fev->comp_sample_ofs + i * 0x0C + 0x04, r->sf);
            string_idx = read_u32le(fev->comp_sample_ofs + i * 0x0C + 0x08, r->sf);
            if (string_idx >= fev->comp_strings)
                return false;

            // still append, could be possible to still point to different names for the same
            // target subsong, either from an earlier comp>sgms check, or from the sound defs
            if (segment_id == target_segment_id && sample_idx == target_sample_idx) {
                name_offset = fev->comp_string_buf + read_u32le(fev->comp_string_ofs + string_idx * 0x04, r->sf);
                read_string(stream_name, STREAM_NAME_SIZE, name_offset, r->sf); // not a size-prefixed FEV string

                if (!strstr(fev->stream_name, stream_name))
                    fev->stream_name_size += append_fev_string(fev->stream_name, STREAM_NAME_SIZE, stream_name, fev->stream_name_size);

                //break; // likely no duplicates of the same target segment id+sample idx?
            }
        }
    }

    return true;
}

static bool parse_fev_composition_segment(fev_header_t* fev, reader_t* r, uint32_t sgms_size) {
    uint32_t sgms_end, chunk_size, entry_ofs;
    uint16_t entries;

    sgms_end = r->offset - 0x08 + sgms_size;
    if (sgms_end > get_streamfile_size(r->sf))
        return false;

    chunk_size = reader_u32(r);
    if (reader_fev_chunk_id(fev, r) != get_id32be("sgmh")) // segment container header
        return false;

    entries = reader_u16(r);
    entry_ofs = r->offset;

    // to simplify things later on, skip ahead all of the segment containers first
    // and check for the presence of "smpf", "str ", "smpm" chunks and store their
    // offsets - they aren't always present [Critter Crunch (PS3), Bolt (PC/X360)]
    for (int i = 0; i < entries; i++) {
        chunk_size = reader_u32(r);
        if (reader_fev_chunk_id(fev, r) != get_id32be("sgmd")) // segment container data
            return false;
        reader_skip(r, chunk_size - 0x08);
    }
    // return false if we're beyond the expected end offset
    // maybe instead use the "cues" chunk for these? (meh)
    if (r->offset >= sgms_end)
        return (r->offset == sgms_end);


    // [Superbrothers: Sword & Sworcery (Android), When Vikings Attack! (PSV)
    //  Marvel Super Hero Squad: Comic Combat (X360), Shank (PS3/X360/PC)]
    chunk_size = reader_u32(r);
    if (reader_fev_chunk_id(fev, r) != get_id32be("smpf")) // sample filenames
        return false;

    chunk_size = reader_u32(r);
    if (reader_fev_chunk_id(fev, r) != get_id32be("str ")) // string data
        return false;
    // 0x00: string count
    // 0x04: string pointers[]
    fev->comp_strings = reader_u32(r);
    fev->comp_string_ofs = r->offset;
    // 0x00: string buffer size
    // 0x04: string buffer[]
    fev->comp_string_buf = fev->comp_string_ofs + fev->comp_strings * 0x04 + 0x04;
    reader_skip(r, chunk_size - 0x0C);

    chunk_size = reader_u32(r);
    if (reader_fev_chunk_id(fev, r) != get_id32be("smpm")) // sample map
        return false;
    // 0x00: sample count
    // 0x04: samples[]
    fev->comp_samples = reader_u32(r);
    // 0x00: segment id
    // 0x04: sample idx
    // 0x08: string idx
    fev->comp_sample_ofs = r->offset;


    // now properly parse these chunks and assign stream names
    r->offset = entry_ofs;
    for (int i = 0; i < entries; i++) {
        uint32_t segment_id, samples;

        chunk_size = reader_u32(r);
        if (reader_fev_chunk_id(fev, r) != get_id32be("sgmd")) // segment container data
            return false;

        // 0x00: segment id
        // 0x04: theme
        // 0x08: (v0x35+) timeline
        // 0x0C: upper (u8), (<v0x33) bank name/subsong
        // 0x0D: lower (u8)
        // 0x0E: bpm
        // 0x12: length
        // 0x16: step sequence
        segment_id = reader_u32(r);
        reader_skip(r, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_53_0)
            reader_skip(r, 0x04);
        if (fev->version <  FMOD_FEV_VERSION_51_0) {
            // moved to the "smp " chunk in later versions
            if (!parse_fev_composition_segment_sample(fev, r, segment_id, 0))
                return false;
        }
        reader_skip(r, 0x0E);

        if (fev->version >= FMOD_FEV_VERSION_51_0) {
            chunk_size = reader_u32(r);
            if (reader_fev_chunk_id(fev, r) != get_id32be("smps")) // sample container
                return false;

            chunk_size = reader_u32(r);
            if (reader_fev_chunk_id(fev, r) != get_id32be("smph")) // sample container header
                return false;

            reader_skip(r, 0x01); // play mode (u8)

            samples = reader_u32(r);
            for (int j = 0; j < samples; j++) {
                chunk_size = reader_u32(r);
                if (reader_fev_chunk_id(fev, r) != get_id32be("smp ")) // sample
                    return false;
                if (!parse_fev_composition_segment_sample(fev, r, segment_id, j))
                    return false;
            }
        }
    }

    // and jump over this to align with the start of the next "comp" chunk
    chunk_size = reader_u32(r);
    if (reader_fev_chunk_id(fev, r) != get_id32be("smpf")) // sample filenames
        return false;
    reader_skip(r, chunk_size - 0x08);

    return (r->offset == sgms_end);
}

static bool parse_fev_composition(fev_header_t* fev, reader_t* r) {
    uint32_t comp_end, chunk_size, chunk_id;

    chunk_size = reader_u32(r);
    chunk_id = reader_fev_chunk_id(fev, r);

    // should just be until EOF
    if (chunk_id != get_id32be("comp")) // composition container header
        return false;
    comp_end = r->offset - 0x08 + chunk_size;
    if (comp_end > get_streamfile_size(r->sf))
        return false;

    // similar to RIFF but reversed size/id order and no chunk alignment
    while (r->offset < comp_end) {
        chunk_size = reader_u32(r);
        chunk_id = reader_fev_chunk_id(fev, r);
        if (r->offset - 0x08 + chunk_size > comp_end)
            return false;

        switch (chunk_id) {
            case 0x73657474: // "sett" music settings
                // 0x00: volume
                // 0x04: reverb
                reader_skip(r, 0x08);
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
                reader_skip(r, chunk_size - 0x08);
                break;

            case 0x73676D73: // "sgms" segment container
                // sub-chunks: "sgmh", "sgmd", "smps", "smph", "smp ", "smpf", "str ", "smpm"
                if (!parse_fev_composition_segment(fev, r, chunk_size))
                    return false;
                break;

            default:
                vgm_logi("FEV: Unknown composition chunk %08X at 0x%X\n", chunk_id, r->offset);
                return false;
        }
    }

    return (r->offset == comp_end);
}


// initial research based on this which appears to be for FEV v0x34~v0x38
// https://github.com/xoreos/xoreos/blob/master/src/sound/fmodeventfile.cpp
// further research from FMOD::EventSystemI::load in Split/Second's fmod_event.dll
// lastly also found this project by putting fmod_event.dll's func name in github search
// https://github.com/barspinoff/bmod/blob/main/tools/fmod_event/src/fmod_eventsystemi.cpp
static bool parse_fev_main(fev_header_t* fev, reader_t* r) {
    uint32_t wave_banks, event_groups, sound_def_defs, sound_defs, reverb_defs;

    // by far the biggest issue with FEV is the lack of pointers to anything,
    // so everything has to be read in sequence until you get to stream names

    // 0x00: (v0x2E+) sound def pool
    // 0x04: (v0x32+) 64-bit sound def pool
    // 0x08: (v0x40) object table[] (FEV1 only, RIFF>LIST>OBCT)
    if (fev->version >= FMOD_FEV_VERSION_46_0)
        reader_skip(r, 0x04); // fmod_event.dll v0x3E still reads this
    if (fev->version >= FMOD_FEV_VERSION_50_0)
        reader_skip(r, 0x04); // fmod_event.dll v0x3E seeks over this
    if (fev->version == FMOD_FEV_VERSION_64_0)
        reader_skip(r, reader_u32(r) * 0x08);

    // FEV bank name (should match filename)
    if (fev->version >= FMOD_FEV_VERSION_25_0) {
        if (!reader_fev_string(NULL, STREAM_NAME_SIZE, r))
            return false;
    }

    fev->languages = 1;
    fev->bank_name_idx = -1;

    // 0x00: sound banks
    // 0x04: (v0x41+) languages (32 max, not enforced?)
    wave_banks = reader_u32(r);
    if (fev->version >= FMOD_FEV_VERSION_65_0)
        fev->languages = reader_u32(r);

    for (int i = 0; i < wave_banks; i++) {
        char wavebank_name[STREAM_NAME_SIZE];
        uint32_t wavebank_offset;

        wavebank_offset = r->offset;
        // 0x00: mode/flags?
        // 0x04: (v0x14+) max streams
        // 0x08: (v0x3D+) hash[langs] (u64)
        // 0x10: (v0x41+) fsb suffix[langs] (interleaved)
        // 0x14: FSB bank name (should match filename)
        reader_skip(r, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_20_0)
            reader_skip(r, 0x04);
        if (fev->version >= FMOD_FEV_VERSION_61_0)
            reader_skip(r, 0x08 * fev->languages);
        if (fev->version >= FMOD_FEV_VERSION_65_0)
            reader_skip(r, 0x04 * fev->languages);
        if (!reader_fev_bankname(wavebank_name, STREAM_NAME_SIZE, r)) // wave bank name
            return false;

        // v0x41+ just store indices later, but this one isn't in RIFF>LIST>STRR
        if (fev->version >= FMOD_FEV_VERSION_65_0) {
            char full_wavebank_name[STREAM_NAME_SIZE];
            char fsb_suffix[STREAM_NAME_SIZE];
            uint32_t string_idx, name_offset;

            // FSB language suffixes aren't seen used very often
            // [Disney/Pixar Brave (PS3) - brave_environment.fev]
            for (int j = 0; j < fev->languages; j++) {
                // points to an empty string if there is no suffix to add
                // otherwise append STRR suffix to match the FSB wavebank
                string_idx = read_u32le(wavebank_offset + 0x08 + j * 0x0C + 0x08, r->sf);
                if (string_idx >= fev->riff_strings)
                    return false;
                name_offset = fev->riff_string_buf + read_u32le(fev->riff_string_ofs + string_idx * 0x04, r->sf);
                read_string(fsb_suffix, STREAM_NAME_SIZE, name_offset, r->sf); // not a size-prefixed FEV string
                snprintf(full_wavebank_name, STREAM_NAME_SIZE, "%s%s", wavebank_name, fsb_suffix);

                if (strncasecmp(full_wavebank_name, fev->fsb_wavebank_name, STREAM_NAME_SIZE) == 0) {
                    if (fev->bank_name_idx != -1)
                        vgm_logi("FEV: Multiple identical bank names\n");
                    fev->bank_name_idx = i;
                    break;
                }
            }
        }
    }

    if (!parse_fev_category(fev, r))
        return false;

    event_groups = reader_u32(r);
    for (int i = 0; i < event_groups; i++) {
        if (!parse_fev_event_category(fev, r))
            return false;
    }

    if (fev->version >= FMOD_FEV_VERSION_46_0) {
        sound_def_defs = reader_u32(r);
        for (int i = 0; i < sound_def_defs; i++)
            parse_fev_sound_def_def(fev, r);
    }

    // a majority of stream names are contained within here
    sound_defs = reader_u32(r);
    for (int i = 0; i < sound_defs; i++) {
        if (!parse_fev_sound_def(fev, r))
            return false;
    }

    if (fev->version >= FMOD_FEV_VERSION_21_0) {
        reverb_defs = reader_u32(r);
        for (int i = 0; i < reverb_defs; i++) {
            if (!parse_fev_reverb_def(fev, r))
                return false;
        }
    }

    // later FEVs also contain stream names within here too
    if (fev->version >= FMOD_FEV_VERSION_47_0) {
        if (!parse_fev_composition(fev, r))
            return false;
    }

    // can rarely have no name in both FSB/FEV [Stoked (X360)]
    return true;
}

static bool parse_fev(fev_header_t* fev, STREAMFILE* sf) {
    uint32_t min_fev_ver, max_fev_ver;
    off_t fev_offset;
    reader_t r = {0};

    if (is_id32be(0x00, sf, "FEV1")) {
        fev->version = read_u32le(0x04, sf); // v0x07~v0x40
        min_fev_ver = FMOD_FEV_VERSION_7_0;
        max_fev_ver = FMOD_FEV_VERSION_64_0;
        fev_offset = 0x08;
    }
    else if (is_id32be(0x00, sf, "RIFF") &&
             is_id32be(0x08, sf, "FEV ") &&
             is_id32be(0x0C, sf, "FMT ")) {
        off_t chunk_offset;

        fev->version = read_u32le(0x14, sf); // v0x41~v0x45
        // find the RIFF>LIST>LGCY chunk which has FEV1 format data
        if (!find_aligned_chunk_le(sf, get_id32be("LIST"), 0x0C, false, &chunk_offset, NULL))
            return false;
        if (!is_id32be(chunk_offset + 0x00, sf, "PROJ"))
            return false;
        if (!find_aligned_chunk_le(sf, get_id32be("LGCY"), chunk_offset + 0x04, false, &chunk_offset, NULL))
            return false;
        fev_offset = chunk_offset;
        // many strings in RIFF FEV are indexed from the STRR chunk
        if (!find_aligned_chunk_le(sf, get_id32be("STRR"), chunk_offset - 0x08, false, &chunk_offset, NULL))
            return false;
        fev->riff_strings = read_u32le(chunk_offset, sf);
        fev->riff_string_ofs = chunk_offset + 0x04;
        fev->riff_string_buf = fev->riff_string_ofs + fev->riff_strings * 0x04;

        min_fev_ver = FMOD_FEV_VERSION_65_0;
        max_fev_ver = FMOD_FEV_VERSION_69_0;
    }
    else
        return false;

    if ((fev->version & 0xFF00FFFF) ||
        fev->version < min_fev_ver ||
        fev->version > max_fev_ver)
        return false;

    reader_setup(&r, sf, fev_offset, false);

    if (!parse_fev_main(fev, &r))
        return false;

    /*
    // appending sound def names in addition to stream names
    if (fev->sound_name[0]) {
        if (fev->sound_def_name[0])
            snprintf(fev->output_stream_name, STREAM_NAME_SIZE, "%s (%s)", fev->sound_name, fev->sound_def_name);
        else
            snprintf(fev->output_stream_name, STREAM_NAME_SIZE, "%s", fev->sound_name);
    }
    else if (fev->sound_def_name[0])
        snprintf(fev->output_stream_name, STREAM_NAME_SIZE, "(%s)", fev->sound_def_name);
    */

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
