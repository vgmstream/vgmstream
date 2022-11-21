#include <math.h>
#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "ubi_sb_streamfile.h"


#define SB_MAX_LAYER_COUNT 16  /* arbitrary max */
#define SB_MAX_CHAIN_COUNT 256 /* +150 exist in Tonic Trouble */
#define SB_MAX_SUBSONGS 128000 /* arbitrary max to detect incorrect reads */

#define LAYER_HIJACK_GRAW_X360  1
#define LAYER_HIJACK_SCPT_PS2   2

typedef enum { UBI_IMA, UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, RAW_AT3, FMT_XMA1, RAW_XMA1, FMT_OGG, FMT_CWAV, FMT_APM, FMT_MPDX, UBI_IMA_SCE } ubi_sb_codec;
typedef enum { UBI_PC, UBI_DC, UBI_PS2, UBI_XBOX, UBI_GC, UBI_X360, UBI_PSP, UBI_PS3, UBI_WII, UBI_3DS } ubi_sb_platform;
typedef enum { UBI_NONE = 0, UBI_AUDIO, UBI_LAYER, UBI_SEQUENCE, UBI_SILENCE } ubi_sb_type;

typedef struct {
    int map_version;
    size_t map_entry_size;
    off_t map_name;
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    size_t resource_name_size;
    size_t blk_table_size;

    off_t audio_extra_offset;
    off_t audio_stream_size;
    off_t audio_stream_offset;
    off_t audio_stream_type;
    off_t audio_software_flag;
    off_t audio_hwmodule_flag;
    off_t audio_streamed_flag;
    off_t audio_cd_streamed_flag;
    off_t audio_loop_flag;
    off_t audio_loc_flag;
    off_t audio_stereo_flag;
    off_t audio_ram_streamed_flag;
    off_t audio_internal_flag;
    off_t audio_num_samples;
    off_t audio_num_samples2;
    off_t audio_sample_rate;
    off_t audio_channels;
    off_t audio_stream_name;
    off_t audio_extra_name;
    off_t audio_xma_offset;
    off_t audio_pitch;
    int audio_streamed_and;
    int audio_cd_streamed_and;
    int audio_loop_and;
    int audio_software_and;
    int audio_hwmodule_and;
    int audio_loc_and;
    int audio_stereo_and;
    int audio_ram_streamed_and;
    int audio_has_internal_names;
    size_t audio_interleave;
    int audio_fix_psx_samples;
    int has_rs_files;

    off_t sequence_extra_offset;
    off_t sequence_sequence_loop;
    off_t sequence_sequence_single;
    off_t sequence_sequence_count;
    off_t sequence_entry_number;
    size_t sequence_entry_size;

    off_t layer_extra_offset;
    off_t layer_layer_count;
    off_t layer_stream_size;
    off_t layer_stream_offset;
    off_t layer_stream_name;
    off_t layer_extra_name;
    off_t layer_sample_rate;
    off_t layer_channels;
    off_t layer_stream_type;
    off_t layer_num_samples;
    off_t layer_pitch;
    off_t layer_loc_flag;
    int layer_loc_and;
    size_t layer_entry_size;
    int layer_hijack;

    off_t silence_duration_int;
    off_t silence_duration_float;

    off_t random_extra_offset;
    off_t random_sequence_count;
    size_t random_entry_size;
    int random_percent_int;

    int is_padded_section1_offset;
    int is_padded_section2_offset;
    int is_padded_section3_offset;
    int is_padded_sectionX_offset;
    int is_padded_sounds_offset;
    int ignore_layer_error;
} ubi_sb_config;

typedef struct {
    ubi_sb_platform platform;
    int is_ps2_old;
    int is_psp_old;
    int big_endian;
    int total_subsongs;
    int bank_subsongs;

    /* SB config */
    /* header varies slightly per game/version but not enough parse case by case,
     * instead we configure sizes and offsets to where each variable is */
    ubi_sb_config cfg;

    /* map base header info */
    off_t map_start;
    uint32_t map_num;

    uint32_t map_type;
    uint32_t map_zero;
    off_t map_offset;
    size_t map_size;
    char map_name[0x28];
    uint32_t map_unknown;

    /* SB info (some values are derived depending if it's standard sbX or map sbX) */
    int is_bank;
    int is_map;
    int is_bnm;
    int is_dat;
    int is_ps2_bnm;
    int is_blk;
    int has_numbered_banks;

    int header_init;
    STREAMFILE* sf_header;
    uint32_t version;           /* 16b+16b major+minor version */
    uint32_t version_empty;     /* map sbX versions are empty */
    /* events (often share header_id/type with some descriptors,
     * but may exist without headers or header exist without them) */
    uint32_t section1_num;
    off_t section1_offset;
    /* descriptors, audio header or other config types */
    uint32_t section2_num;
    off_t section2_offset;
    /* internal streams table, referenced by each header */
    uint32_t section3_num;
    off_t section3_offset;
    /* section with sounds in some map versions */
    uint32_t section4_num;
    off_t section4_offset;
    /* extra table, config for certain types (DSP coefs, external resources, layer headers, etc) */
    uint32_t sectionX_size;
    off_t sectionX_offset;
    /* sound bank size */
    size_t bank_size;
    /* BNM bank number */
    int bank_number;
    /* unknown, usually -1 but can be others (0/1/2/etc) */
    int flag1;
    int flag2;

    /* header/stream info */
    ubi_sb_type type;           /* unified type */
    ubi_sb_codec codec;         /* unified codec */
    int header_index;           /* entry number within section2 */
    off_t header_offset;        /* entry offset within section2 */
    uint32_t header_id;         /* 16b+16b group+sound identifier (unique within a sbX, but not smX), may start from 0 */
    uint32_t header_type;       /* parsed type (we only need audio types) */
    off_t extra_offset;         /* offset within sectionX to extra data */
    off_t stream_offset;        /* offset within the data section (internal) or absolute (external) to the audio */
    size_t stream_size;         /* size of the audio data */
    uint32_t stream_type;       /* rough codec value */
    uint32_t subblock_id;       /* internal id to reference in section3 */
    uint8_t subbank_index;      /* ID of the entry in DC bank */

    int loop_flag;              /* stream loops (normally internal sfx, but also external music) */
    int loop_start;             /* usually 0 */
    int num_samples;            /* should match manually calculated samples */
    int sample_rate;
    int channels;
    off_t xma_header_offset;    /* some XMA have extra header stuff */

    int layer_count;            /* number of layers in a layer type */
    int layer_channels[SB_MAX_LAYER_COUNT];
    int sequence_count;         /* number of segments in a sequence type */
    int sequence_chain[SB_MAX_CHAIN_COUNT]; /* sequence of entry numbers */
    int sequence_banks[SB_MAX_CHAIN_COUNT]; /* sequence of bnk bank numbers */
    int sequence_multibank;     /* info flag */
    int sequence_loop;          /* chain index to loop */
    int sequence_single;        /* if que sequence plays once (loops by default) */

    float duration;             /* silence duration */

    int is_streamed;            /* sound is streamed from storage */
    int is_cd_streamed;         /* found in PS2 BNM */
    int is_ram_streamed;        /* found in some PS2 games */
    int is_external;            /* sound is in an external file */
    int is_localized;           /* found in old PS2 games, determines which file the sound is in */
    char resource_name[0x28];   /* filename to the external stream, or internal stream info for some games */

    char readable_name[255];    /* final subsong name */
    int types[16];              /* counts each header types, for debugging */
    int allowed_types[16];
} ubi_sb_header;

static int parse_bnm_header(ubi_sb_header* sb, STREAMFILE* sf);
static int parse_bnm_ps2_header(ubi_sb_header* sb, STREAMFILE* sf);
static int parse_dat_header(ubi_sb_header *sb, STREAMFILE *sf);
static int parse_header(ubi_sb_header* sb, STREAMFILE* sf, off_t offset, int index);
static int parse_sb(ubi_sb_header* sb, STREAMFILE* sf, int target_subsong);
static VGMSTREAM* init_vgmstream_ubi_sb_header(ubi_sb_header* sb, STREAMFILE* sf_index, STREAMFILE* sf);
static VGMSTREAM *init_vgmstream_ubi_sb_silence(ubi_sb_header *sb);
static int config_sb_platform(ubi_sb_header* sb, STREAMFILE* sf);
static int config_sb_version(ubi_sb_header* sb, STREAMFILE* sf);
static int init_sb_header(ubi_sb_header* sb, STREAMFILE* sf);


/* .SBx - banks from Ubisoft's DARE (Digital Audio Rendering Engine) engine games in ~2000-2008+ */
VGMSTREAM* init_vgmstream_ubi_sb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_index = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = {0};
    int target_subsong = sf->stream_index;


    /* checks (number represents the platform, see later) */
    if (!check_extensions(sf, "sb0,sb1,sb2,sb3,sb4,sb5,sb6,sb7"))
        goto fail;

    /* .sbX (sound bank) is a small multisong format (loaded in memory?) that contains SFX data
     * but can also reference .ss0/ls0 (sound stream) external files for longer streams.
     * A companion .sp0 (sound project) describes files and if it uses BANKs (.sbX) or MAPs (.smX). */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, sf))
        goto fail;
    read_32bit = sb.big_endian ? read_32bitBE : read_32bitLE;

    if (target_subsong <= 0) target_subsong = 1;

    /* use smaller header buffer for performance */
    sf_index = reopen_streamfile(sf, 0x100);
    if (!sf_index) goto fail;


    /* SB HEADER */
    /* SBx layout: header, section1, section2, extra section, section3, data (all except header can be null) */
    sb.is_bank = 1;
    sb.version = read_32bit(0x00, sf);

    if (!config_sb_version(&sb, sf))
        goto fail;
    if (!init_sb_header(&sb, sf))
        goto fail;


    if (sb.cfg.is_padded_section1_offset)
        sb.section1_offset = align_size_to_block(sb.section1_offset, 0x10);

    sb.section2_offset = sb.section1_offset + sb.cfg.section1_entry_size * sb.section1_num;
    if (sb.cfg.is_padded_section2_offset)
        sb.section2_offset = align_size_to_block(sb.section2_offset, 0x10);

    sb.sectionX_offset = sb.section2_offset + sb.cfg.section2_entry_size * sb.section2_num;
    if (sb.cfg.is_padded_sectionX_offset)
        sb.sectionX_offset = align_size_to_block(sb.sectionX_offset, 0x10);

    sb.section3_offset = sb.sectionX_offset + sb.sectionX_size;
    if (sb.cfg.is_padded_section3_offset)
        sb.section3_offset = align_size_to_block(sb.section3_offset, 0x10);

    if (!parse_sb(&sb, sf_index, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, sf_index, sf);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_index);
    return NULL;
}

/* .SMx - maps (sets of custom SBx files) also from Ubisoft's sound engine games in ~2000-2008+ */
VGMSTREAM* init_vgmstream_ubi_sm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_index = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = {0}, target_sb = {0};
    int target_subsong = sf->stream_index;
    int i;


    /* checks (number represents platform, lmX are localized variations) */
    if (!check_extensions(sf, "sm0,sm1,sm2,sm3,sm4,sm5,sm6,sm7,lm0,lm1,lm2,lm3,lm4,lm5,lm6,lm7"))
        goto fail;

    /* .smX (sound map) is a set of slightly different sbX files, compiled into one "map" file.
     * Map has a sbX (called "submap") per named area (example: menu, level1, boss1, level2...).
     * This counts subsongs from all sbX, so totals can be massive, but there are splitters into mini-smX. */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, sf))
        goto fail;
    read_32bit = sb.big_endian ? read_32bitBE : read_32bitLE;

    if (target_subsong <= 0) target_subsong = 1;

    /* use smaller header buffer for performance */
    sf_index = reopen_streamfile(sf, 0x100);
    if (!sf_index) goto fail;


    /* SM BASE HEADER */
    /* SMx layout: header with N map area offset/sizes + custom SBx with relative offsets */
    sb.is_map = 1;
    sb.version   = read_32bit(0x00, sf);
    sb.map_start = read_32bit(0x04, sf);
    sb.map_num   = read_32bit(0x08, sf);

    if (!config_sb_version(&sb, sf))
        goto fail;


    for (i = 0; i < sb.map_num; i++) {
        off_t offset = sb.map_start + i * sb.cfg.map_entry_size;

        /* SUBMAP HEADER */
        sb.map_type     = read_32bit(offset + 0x00, sf); /* usually 0/1=first, 0=rest */
        sb.map_zero     = read_32bit(offset + 0x04, sf);
        sb.map_offset   = read_32bit(offset + 0x08, sf);
        sb.map_size     = read_32bit(offset + 0x0c, sf); /* includes sbX header, but not internal streams */
        read_string(sb.map_name, sizeof(sb.map_name), offset + sb.cfg.map_name, sf); /* null-terminated and may contain garbage after null */
        if (sb.cfg.map_version >= 3)
            sb.map_unknown  = read_32bit(offset + 0x30, sf); /* uncommon, id/config? longer name? mem garbage? */

        /* SB HEADER */
        /* SBx layout: base header, section1, section2, section4, extra section, section3, data (all except header can be null?) */
        sb.version_empty    = read_32bit(sb.map_offset + 0x00, sf); /* sbX in maps don't set version */
        sb.section1_offset  = read_32bit(sb.map_offset + 0x04, sf) + sb.map_offset;
        sb.section1_num     = read_32bit(sb.map_offset + 0x08, sf);
        sb.section2_offset  = read_32bit(sb.map_offset + 0x0c, sf) + sb.map_offset;
        sb.section2_num     = read_32bit(sb.map_offset + 0x10, sf);

        if (sb.cfg.map_version < 3) {
            sb.section3_offset  = read_32bit(sb.map_offset + 0x14, sf) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x18, sf);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x1c, sf) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x20, sf);
        } else {
            sb.section4_offset  = read_32bit(sb.map_offset + 0x14, sf);
            sb.section4_num     = read_32bit(sb.map_offset + 0x18, sf);
            sb.section3_offset  = read_32bit(sb.map_offset + 0x1c, sf) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x20, sf);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x24, sf) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x28, sf);

            /* latest map format has another section with sounds after section 2 */
            sb.section2_num    += sb.section4_num;    /* let's just merge it with section 2 */
            sb.sectionX_offset += sb.section4_offset; /* for some reason, this is relative to section 4 here */
        }

        VGM_ASSERT(sb.map_type != 0 && sb.map_type != 1, "UBI SM: unknown map_type at %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.map_zero != 0, "UBI SM: unknown map_zero at %x\n", (uint32_t)offset);
        //;VGM_ASSERT(sb.map_unknown != 0, "UBI SM: unknown map_unknown at %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.version_empty != 0, "UBI SM: unknown version_empty at %x\n", (uint32_t)offset);

        if (!parse_sb(&sb, sf_index, target_subsong))
            goto fail;

        /* snapshot of current sb if subsong was found
         * (it gets rewritten and we need exact values for sequences and stuff) */
        if (sb.type != UBI_NONE) {
            target_sb = sb; /* memcpy */
            sb.type = UBI_NONE; /* reset parsed flag */
        }
    }

    target_sb.total_subsongs = sb.total_subsongs;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&target_sb, sf_index, sf);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_index);
    return NULL;
}


/* .BNM - proto-sbX with map style format [Rayman 2 (PC), Donald Duck: Goin' Quackers (PC), Tonic Trouble (PC)] */
VGMSTREAM* init_vgmstream_ubi_bnm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_index = NULL;
    ubi_sb_header sb = {0};
    int target_subsong = sf->stream_index;

    if (target_subsong <= 0) target_subsong = 1;


    /* checks */
    if (!check_extensions(sf, "bnm"))
        goto fail;

    /* v0, header is somewhat like a map-style bank (offsets + sizes) but sectionX/3 fields are
     * fixed/reserved. Header entry sizes and config works the same, and type numbers are slightly
     * different, but otherwise pretty much the same engine (not named DARE yet). Curiously, it may
     * stream RIFF .wav (stream_offset pointing to "data"), and also .raw (PCM) or .apm IMA. */

    if (!parse_bnm_header(&sb, sf))
        goto fail;

    /* use smaller header buffer for performance */
    sf_index = reopen_streamfile(sf, 0x100);
    if (!sf_index) goto fail;

    if (!parse_sb(&sb, sf_index, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, sf_index, sf);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_index);
    return NULL;
}

static int parse_bnm_header(ubi_sb_header* sb, STREAMFILE* sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;

    /* PLATFORM DETECTION */
    sb->platform = UBI_PC;
    sb->big_endian = 0;
    read_32bit = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* SB HEADER */
    /* SBx layout: header, section1, section2, extra section, section3, data (all except header can be null) */
    sb->is_bnm = 1;
    sb->version          = read_32bit(0x00, sf);
    if (!config_sb_version(sb, sf))
        goto fail;

    sb->section1_offset  = read_32bit(0x04, sf);
    sb->section1_num     = read_32bit(0x08, sf);
    sb->section2_offset  = read_32bit(0x0c, sf);
    sb->section2_num     = read_32bit(0x10, sf);
    sb->section3_offset  = read_32bit(0x14, sf);
    sb->section3_num     = 0;

    sb->sectionX_offset  = sb->section2_offset + sb->section2_num * sb->cfg.section2_entry_size;
    sb->sectionX_size    = sb->section3_offset - sb->sectionX_offset;

    return 1;
fail:
    return 0;
}

static int bnm_parse_offsets(ubi_sb_header *sb, STREAMFILE *sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE *) = sb->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t block_offset;

    if (sb->is_external)
        return 1;

    /* sounds are split into subblocks based on resource type and codec, the order is hardcoded */
    if (sb->version == 0x00000000 || sb->version == 0x00000200) {
        /* 0x14: MPDX, 0x18: MIDI, 0x1c: PCM, 0x20: APM, 0x24: streamed, 0x28: EOF */
        switch (sb->stream_type) {
            case 0x01:
                block_offset = read_32bit(0x1c, sf);
                break;
            case 0x02:
                block_offset = read_32bit(0x14, sf);
                break;
            case 0x04:
                block_offset = read_32bit(0x20, sf);
                break;
            default:
                goto fail;
        }
    } else if (sb->version == 0x00060409) {
        /* The Jungle Book is stripped down compared to other versions */
        /* 0x14: Ubi ADPCM, 0x18: PCM, 0x1c: streamed */
        switch (sb->stream_type) {
            case 0x01:
                block_offset = read_32bit(0x18, sf);
                break;
            case 0x06:
                block_offset = read_32bit(0x14, sf);
                break;
            default:
                goto fail;
        }
    } else {
        VGM_LOG("UBI BNM: Unknown subblock offsets for version %08x", sb->version);
        goto fail;
    }

    sb->stream_offset += block_offset;

    return 1;
fail:
    return 0;
}

static int parse_ubi_bank_header(ubi_sb_header *sb, ubi_sb_header *sb_other, STREAMFILE *sf) {
    if (sb->is_bnm) {
        return parse_bnm_header(sb_other, sf);
    } else if (sb->is_dat) {
        return parse_dat_header(sb_other, sf);
    } else if (sb->is_ps2_bnm) {
        return parse_bnm_ps2_header(sb_other, sf);
    }

    return 0;
}

static void get_ubi_bank_name(ubi_sb_header *sb, int bank_number, char *bank_name) {
    if (sb->is_bnm) {
        sprintf(bank_name, "Bnk_%d.bnm", bank_number);
    } else if (sb->is_dat) {
        sprintf(bank_name, "BNK_%d.DAT", bank_number);
    } else if (sb->is_ps2_bnm) {
        sprintf(bank_name, "BNK_%d.BNM", bank_number);
    } else {
        strcpy(bank_name, "ERROR");
    }
}

static int is_other_bank(ubi_sb_header *sb, STREAMFILE *sf, int bank_number) {
    char current_name[PATH_LIMIT];
    char bank_name[255];

    get_streamfile_filename(sf, current_name, PATH_LIMIT);
    get_ubi_bank_name(sb, bank_number, bank_name);

    return strcmp(current_name, bank_name) != 0;
}

/* .DAT - very similar to BNM, used on Dreamcast */
VGMSTREAM *init_vgmstream_ubi_dat(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *sf_index = NULL;
    ubi_sb_header sb = { 0 };
    int target_subsong = sf->stream_index;

    if (target_subsong <= 0) target_subsong = 1;

    /* checks */
    if (!check_extensions(sf, "dat"))
        goto fail;

    if (!parse_dat_header(&sb, sf))
        goto fail;

    /* use smaller header buffer for performance */
    sf_index = reopen_streamfile(sf, 0x100);
    if (!sf_index) goto fail;

    if (!parse_sb(&sb, sf_index, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, sf_index, sf);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_index);
    return NULL;
}

static int parse_dat_header(ubi_sb_header *sb, STREAMFILE *sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE *) = NULL;

    /* only used on DC */
    sb->platform = UBI_DC;
    sb->big_endian = 0;
    read_32bit = sb->big_endian ? read_32bitBE : read_32bitLE;

    sb->is_dat = 1;
    sb->version         = read_32bit(0x00, sf);
    if (sb->version != 0x00000000)
        goto fail;

    if (!config_sb_version(sb, sf))
        goto fail;

    sb->section1_offset = read_32bit(0x04, sf);
    sb->section1_num    = read_32bit(0x08, sf);
    sb->section2_offset = read_32bit(0x0c, sf);
    sb->section2_num    = read_32bit(0x10, sf);
    sb->bank_size       = read_32bit(0x14, sf);

    if (sb->section1_offset != 0x18)
        goto fail;

    if (sb->section2_offset != sb->section1_offset + sb->section1_num * sb->cfg.section1_entry_size)
        goto fail;

    if (sb->bank_size != get_streamfile_size(sf))
        goto fail;

    sb->sectionX_offset = sb->section2_offset + sb->section2_num * sb->cfg.section2_entry_size;
    sb->sectionX_size   = sb->bank_size - sb->sectionX_offset;

    return 1;
fail:
    return 0;
}

static VGMSTREAM *init_vgmstream_ubi_dat_main(ubi_sb_header *sb, STREAMFILE *sf_index, STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *sf_data = NULL;

    if (sb->is_external) {
        sf_data = open_streamfile_by_filename(sf, sb->resource_name);
        if (!sf_data) {
            /* play silence if external file is not found since Rayman 2 seems to rely on this behavior */
            vgm_logi("UBI DAT: external file '%s' not found (put together)\n", sb->resource_name);
            concatn(sizeof(sb->readable_name), sb->readable_name, " (missing)");
            sb->duration = (float)pcm_bytes_to_samples(sb->stream_size, sb->channels, 16) / (float)sb->sample_rate;
            return init_vgmstream_ubi_sb_silence(sb);
        }
    }

    /* DAT banks don't work with raw audio data, they open full external files and rely almost entirely
     * on their metadata, that's why we're handling this here, separately from other types */
    switch (sb->stream_type) {
        case 0x01: {
            if (!sb->is_external) { /* Dreamcast bank */
                if (sb->version == 0x00000000) {
                    uint32_t entry_offset, start_offset, num_samples, codec;
                    uint8_t buf[4];

                    sf_data = open_streamfile_by_ext(sf, "osb");
                    if (!sf_data) {
                        VGM_LOG("UBI DAT: no matching OSB found\n");
                        goto fail;
                    }

                    /* FIXME: hacky handling of OSB bank, need to eventually write a full parser once
                     * the format is fully cracked */
                    entry_offset = read_32bitLE(0x10 + sb->subbank_index * 0x04, sf_data);

                    /* stores values in a weird zig-zag pattern */
                    if (read_streamfile(buf, entry_offset + 0x04, 4, sf_data) != 4) goto fail;
                    start_offset = (buf[0] << 16) | (buf[2]) | (buf[3] << 8);
                    if (read_streamfile(buf, entry_offset + 0x08, 4, sf_data) != 4) goto fail;
                    num_samples = (buf[0] << 16) | (buf[1] << 24) | (buf[2]) | (buf[3] << 8);
                    num_samples /= sb->channels;
                    codec = read_8bit(entry_offset + 0x05, sf_data);

                    /* build the VGMSTREAM */
                    vgmstream = allocate_vgmstream(sb->channels, sb->loop_flag);
                    if (!vgmstream) goto fail;

                    if (codec == 0) {
                        vgmstream->coding_type = coding_PCM16LE;
                        vgmstream->layout_type = layout_interleave;
                        vgmstream->interleave_block_size = 0x02;
                        vgmstream->stream_size = num_samples * sb->channels * 2;
                    } else {
                        vgmstream->coding_type = coding_AICA_int;
                        vgmstream->layout_type = layout_interleave;
                        vgmstream->interleave_block_size = 0x01;
                        vgmstream->stream_size = num_samples * sb->channels / 2;
                    }

                    vgmstream->num_samples = num_samples;
                    vgmstream->loop_start_sample = sb->loop_start;
                    vgmstream->loop_end_sample = vgmstream->num_samples;

                    if (!vgmstream_open_stream(vgmstream, sf_data, start_offset))
                        goto fail;
                } else if (sb->version == 0x00000200) {
                    sf_data = open_streamfile_by_ext(sf, "kat");
                    if (!sf_data) {
                        VGM_LOG("UBI DAT: no matching KAT found\n");
                        goto fail;
                    }

                    /* KAT defines its own loop points */
                    sf_data->stream_index = sb->subbank_index + 1;
                    vgmstream = init_vgmstream_kat(sf_data);
                    if (!vgmstream) goto fail;
                } else {
                    goto fail;
                }
            } else { /* raw PCM */
                vgmstream = allocate_vgmstream(sb->channels, sb->loop_flag);
                if (!vgmstream) goto fail;

                /* TODO: some WAVs pop at the end because of LIST chunk, doesn't happen in-game [Donald Duck (DC)] */
                vgmstream->coding_type = coding_PCM16LE;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x02;
                vgmstream->num_samples = pcm_bytes_to_samples(sb->stream_size, sb->channels, 16);
                vgmstream->loop_start_sample = sb->loop_start;
                vgmstream->loop_end_sample = vgmstream->num_samples;

                if (!vgmstream_open_stream(vgmstream, sf_data, sb->stream_offset))
                    goto fail;
            }
            break;
        }
        case 0x04: { /* standard WAV */
            if (!sb->is_external) {
                VGM_LOG("UBI DAT: Found RAM stream_type 0x04\n");
                goto fail;
            }

            vgmstream = init_vgmstream_riff(sf_data);
            if (!vgmstream) goto fail;
            break;
        }
        default:
            VGM_LOG("UBI DAT: Unkown stream_type %d\n", sb->stream_type);
            goto fail;
    }

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->num_streams = sb->total_subsongs;
    vgmstream->sample_rate = sb->sample_rate;

    close_streamfile(sf_data);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_data);
    return NULL;
}

/* .BNM - used in the earliest PS2 games */
VGMSTREAM *init_vgmstream_ubi_bnm_ps2(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *sf_index = NULL;
    ubi_sb_header sb = { 0 };
    int target_subsong = sf->stream_index;

    if (target_subsong <= 0) target_subsong = 1;

    /* checks */
    if (!check_extensions(sf, "bnm"))
        goto fail;

    if (!parse_bnm_ps2_header(&sb, sf))
        goto fail;

    /* use smaller header buffer for performance */
    sf_index = reopen_streamfile(sf, 0x100);
    if (!sf_index) goto fail;

    if (!parse_sb(&sb, sf_index, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, sf_index, sf);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_index);
    return NULL;
}

static int parse_bnm_ps2_header(ubi_sb_header* sb, STREAMFILE* sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;

    sb->platform = UBI_PS2;
    sb->big_endian = 0;
    read_32bit = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* SB HEADER */
    /* SBx layout: header, section1, section2, extra section, section3, data (all except header can be null) */
    sb->is_ps2_bnm = 1;
    sb->version         = read_32bit(0x00, sf);
    if (sb->version != 0x32787370) /* "psx2" */
        goto fail;

    if (!config_sb_version(sb, sf))
        goto fail;

    sb->bank_number     = read_32bit(0x04, sf);
    sb->section1_offset = read_32bit(0x08, sf);
    sb->section1_num    = read_32bit(0x0c, sf);
    sb->section2_offset = read_32bit(0x10, sf);
    sb->section2_num    = read_32bit(0x14, sf);
    sb->sectionX_offset = read_32bit(0x18, sf);
    sb->bank_size       = read_32bit(0x1c, sf);
    sb->flag1           = read_32bit(0x20, sf);

    sb->sectionX_size   = sb->bank_size - sb->sectionX_offset;

    return 1;
fail:
    return 0;
}

/* .BLK - maps in separate .blk chunks [Donald Duck: Goin' Quackers (PS2), The Jungle Book: Rhythm N'Groove (PS2)] */
VGMSTREAM* init_vgmstream_ubi_blk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_res = NULL, *sf_index = NULL;
    ubi_sb_header sb = { 0 };
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    int target_subsong = sf->stream_index;

    /* Somewhat equivalent to a v0x00000003 map:
     * - HEADER.BLK: base map header + submaps headers
     * - EVT.BLK: section1
     * - RES.BLK: section2 + sectionX
     * - MAP.BLK, MAPLANG.BLK: section3's for each map
     * - STREAMED.BLK, STRLANG.BLK: streamed sounds
     *
     * The format is different from SMx in that there's a single sec1 and sec2
     * shared by all maps so we can't determine which sounds belong to which map.
     * Meanwhile, RAM sounds are stored in MAP.BLK/MAPLANG.BLK, which are split into blocks,
     * one per map, which contain all RAM sounds that should be loaded for a given map.
     * 0x00: version
     * 0x04: number of maps
     * 0x08: number of events (EVT.BLK)
     * 0x0c: number of resources (RES.BLK)
     * 0x10: flags?
     * 0x14: size of extra section in RES.BLK
     * for each map:
     *   0x00: total size of common RAM sounds
     *   0x04: total size of localized RAM sounds
     *   0x08: offset of common sound table in MAP.BLK
     *   0x0c: offset of localized sound table in MAPLANG.BLK
     *   0x10: map name (0x20 bytes)
     */

    /* checks */
    if (!check_extensions(sf, "blk"))
        goto fail;

    /* only known to be used on PS2 */
    sb.platform = UBI_PS2;
    sb.big_endian = 0;
    read_32bit = sb.big_endian ? read_32bitBE : read_32bitLE;

    /* must open HEADER.BLK */
    sb.is_blk = 1;
    sb.version = read_32bit(0x00, sf) & 0x7FFFFFFF;
    if (read_32bit(0x00, sf) & 0x80000000) {
        sb.cfg.blk_table_size = 0x2000;
    } else {
        sb.cfg.blk_table_size = 0x1800;
    }
    if (sb.version != 0x00000003)
        goto fail;

    if (!config_sb_version(&sb, sf))
        goto fail;

    sb.sf_header = sf;
    sb.map_num = read_32bit(0x04, sf);
    sb.section1_num = read_32bit(0x08, sf);
    sb.section1_offset = 0;
    sb.section2_num = read_32bit(0x0c, sf);
    sb.section2_offset = 0;
    sb.sectionX_offset = sb.section2_num * sb.cfg.section2_entry_size;
    sb.sectionX_size = read_32bit(0x14, sf);

    /* ugh... */
    sf_res = open_streamfile_by_filename(sf, "RES.BLK");
    sf_index = reopen_streamfile(sf_res, 0x100);
    if (target_subsong == 0) target_subsong = 1;

    if (!parse_sb(&sb, sf_index, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, sf_index, sf_res);
    close_streamfile(sf_res);
    close_streamfile(sf_index);
    return vgmstream;

fail:
    close_streamfile(sf_res);
    close_streamfile(sf_index);
    return NULL;
}

static int blk_parse_offsets(ubi_sb_header* sb) {
    uint32_t i;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* correct offsets */
    if (sb->is_streamed) {
        /* offsets for streamed sounds are stored in sectors */
        sb->stream_offset *= 0x800;
    } else {
        STREAMFILE* sf_snd = NULL;

        /* find the first map block which has this sound */
        sf_snd = open_streamfile_by_filename(sb->sf_header, sb->resource_name);
        if (!sf_snd) goto fail;
        for (i = 0; i < sb->map_num; i++) {
            uint32_t entry_offset, cmn_table_offset, loc_table_offset, table_offset;
            entry_offset = 0x18 + i * sb->cfg.map_entry_size;
            cmn_table_offset = read_32bit(entry_offset + 0x08, sb->sf_header);
            loc_table_offset = read_32bit(entry_offset + 0x0c, sb->sf_header);
            table_offset = sb->is_localized ? loc_table_offset : cmn_table_offset;

            sb->stream_offset = read_32bit(table_offset + sb->header_index * 0x04, sf_snd);
            if (sb->stream_offset != 0xFFFFFFFF) {
                sb->stream_offset += table_offset + sb->cfg.blk_table_size;
                //sb->stream_size -= 0x04;
                break;
            }
        }
        close_streamfile(sf_snd);

        if (sb->stream_offset == 0xFFFFFFFF) {
            VGM_LOG("UBI BLK: No map block contains resource %08x (%d)\n", sb->header_id, sb->header_index);
            goto fail;
        }
    }

    return 1;
fail:
    return 0;
}

static void blk_get_resource_name(ubi_sb_header* sb) {
    if (sb->is_streamed) {
        if (sb->is_localized) {
            strcpy(sb->resource_name, "STRLANG.BLK");
        } else {
            strcpy(sb->resource_name, "../STREAMED.BLK");
        }
    } else {
        if (sb->is_localized) {
            strcpy(sb->resource_name, "MAPLANG.BLK");
        } else {
            strcpy(sb->resource_name, "../MAP.BLK");
        }
    }
}

/* ************************************************************************* */

static VGMSTREAM* init_vgmstream_ubi_sb_base(ubi_sb_header* sb, STREAMFILE* sf_head, STREAMFILE* sf_data, off_t start_offset) {
    VGMSTREAM* vgmstream = NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sb->channels, sb->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = sb->sample_rate;
    vgmstream->num_streams = sb->total_subsongs;
    vgmstream->stream_size = sb->stream_size;

    vgmstream->num_samples = sb->num_samples;
    vgmstream->loop_start_sample = sb->loop_start;
    vgmstream->loop_end_sample = sb->num_samples;

    switch(sb->codec) {
        case UBI_IMA:
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case UBI_IMA_SCE:
            vgmstream->coding_type = coding_UBI_SCE_IMA;
            vgmstream->layout_type = layout_blocked_ubi_sce;
            vgmstream->full_block_size = read_32bitLE(0x18, sf_data);

            /* this "codec" is an ugly hack of IMA w/ Ubi ADPCM's frame format, surely to
             * shoehorn a simpler codec into the existing code when porting the game */
            start_offset += 0x08 + 0x30; /* skip Ubi ADPCM header */
            break;

        case UBI_ADPCM:
            /* custom Ubi 4/6-bit ADPCM used in early games:
             * - Splinter Cell (PC): 4-bit w/ 1ch/2ch (all streams + menu music)
             * - Batman: Vengeance (PC): 4-bit w/ 1ch/2ch (all streams)
             * - Myst IV (PC/Xbox): 4-bit w/ 1ch (amb), some files only (ex. sfx_si_puzzle_stream.sb2)
             * - The Jungle Book: Rhythm N'Groove (PC): 4-bit w/ 2ch (music/amb), 6-bit w/ 1ch (speech)
             * - possibly others
             * internal extension is .adp, maybe this can be called FMT_ADP */

            /* skip extra header (some kind of id?) found in Myst IV */
            if (read_32bitBE(start_offset + 0x00, sf_data) != 0x08000000 &&
                read_32bitBE(start_offset + 0x08, sf_data) == 0x08000000) {
                start_offset += 0x08;
                sb->stream_size -= 0x08;
            }

            vgmstream->codec_data = init_ubi_adpcm(sf_data, start_offset, 0, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_UBI_ADPCM;
            vgmstream->layout_type = layout_none;
            break;

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            if (vgmstream->num_samples == 0) { /* happens in .bnm */
                vgmstream->num_samples       = pcm_bytes_to_samples(sb->stream_size, sb->channels, 16);
                vgmstream->loop_end_sample   = vgmstream->num_samples;
            }
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;

            if (sb->cfg.has_rs_files) {
                /* SC:PT PS2 has extra 0x30 bytes, presumably from (missing) VAG header */
                sb->stream_size -= 0x30;
                vgmstream->stream_size -= 0x30;
            }

            if (sb->is_ps2_bnm) {
                vgmstream->interleave_block_size = (sb->is_cd_streamed) ?
                    sb->cfg.audio_interleave :
                    sb->stream_size / sb->channels;
            } else {
                vgmstream->interleave_block_size = (sb->cfg.audio_interleave) ?
                    sb->cfg.audio_interleave :
                    sb->stream_size / sb->channels;
            }

            if (vgmstream->num_samples == 0) { /* early PS2 games may not set it for internal streams */
                vgmstream->num_samples = ps_bytes_to_samples(sb->stream_size, sb->channels);

                if (sb->loop_start == 0) {
                    ps_find_loop_offsets(sf_data, sb->stream_offset, sb->stream_size,
                        sb->channels, vgmstream->interleave_block_size,
                        &vgmstream->loop_start_sample, &vgmstream->loop_end_sample);
                }
            }

            /* late PS3 SBs have double sample count here for who knows why
             * (loops or not, PS-ADPCM only, possibly only when using codec 0x02 for RAW_PSX) */
            if (sb->cfg.audio_fix_psx_samples) {
                vgmstream->num_samples /= sb->channels;
                vgmstream->loop_start_sample /= sb->channels;
                vgmstream->loop_end_sample /= sb->channels;
            }

            break;

        case RAW_XBOX:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = align_size_to_block(sb->stream_size / sb->channels, 0x08); /* frame-aligned */

            /* mini DSP header (first 0x10 seem to contain DSP header fields like nibbles and format) */
            dsp_read_coefs_be(vgmstream, sf_head, sb->extra_offset + 0x10, 0x40);
            dsp_read_hist_be (vgmstream, sf_head, sb->extra_offset + 0x34, 0x40); /* after gain/initial ps */
            break;

        case FMT_VAG:
            /* skip VAG header (some sb4 use VAG and others raw PSX) */
            if (read_32bitBE(start_offset, sf_data) == 0x56414770) { /* "VAGp" */
                start_offset += 0x30;
                sb->stream_size  -= 0x30;
            }

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = sb->stream_size / sb->channels;
            break;

#ifdef VGM_USE_FFMPEG
        case FMT_AT3: {
            /* skip weird value (3 or 4) in Brothers in Arms: D-Day (PSP) */
            if (read_32bitBE(start_offset+0x04,sf_data) == 0x52494646) {
                VGM_LOG("UBI SB: skipping unknown value 0x%x before RIFF\n", read_32bitBE(start_offset+0x00,sf_data));
                start_offset += 0x04;
                sb->stream_size -= 0x04;
            }

            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf_data, start_offset, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case RAW_AT3: {
            int block_align, encoder_delay;

            block_align = 0x98 * sb->channels;
            encoder_delay = 1024 + 69*2; /* approximate */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf_data, start_offset, sb->stream_size, sb->num_samples, sb->channels, sb->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        // TODO: Ubi XMA1 (raw or fmt) is a bit strange, FFmpeg decodes some frames slightly wrong
        // XMA1 normally has a frame counter in the first nibble but Ubi's is always set to 0.
        // Probably a beta/custom encoder that creates some buggy frames, that a real X360 handles ok, but trips FFmpeg
        // xmaencode decodes correctly if counters are fixed (otherwise has clicks on every frame).
        case FMT_XMA1: {
            uint8_t buf[0x100];
            uint32_t sec1_num, sec2_num, sec3_num, bits_per_frame;
            uint8_t flag;
            size_t bytes, chunk_size, header_size, data_size;
            off_t header_offset;

            chunk_size = 0x20;

            /* formatted XMA sounds have a strange custom header */
            header_offset = start_offset; /* XMA fmt chunk at the start */
            flag = read_8bit(header_offset + 0x20, sf_data);
            sec2_num = read_32bitBE(header_offset + 0x24, sf_data); /* number of XMA frames */
            sec1_num = read_32bitBE(header_offset + 0x28, sf_data);
            sec3_num = read_32bitBE(header_offset + 0x2c, sf_data);

            bits_per_frame = 4;
            if (flag == 0x02 || flag == 0x04)
                bits_per_frame = 2;
            else if (flag == 0x08)
                bits_per_frame = 1;

            header_size = 0x30;
            header_size += sec1_num * 0x04;
            header_size += align_size_to_block(sec2_num * bits_per_frame, 32) / 8; /* bitstream seek table? */
            header_size += sec3_num * 0x08;
            start_offset += header_size;
            data_size = sec2_num * 0x800;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, data_size, sf_data, 1);

            vgmstream->codec_data = init_ffmpeg_header_offset(sf_data, buf, bytes, start_offset, data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_ch(vgmstream, sf_data, start_offset, data_size, sb->channels, 0, 0);
            break;
        }

        case RAW_XMA1: {
            uint8_t buf[0x100];
            size_t bytes, chunk_size;
            off_t header_offset;

            VGM_ASSERT(sb->is_streamed, "UBI SB: Raw XMA used for streamed sound\n");

            /* get XMA header from extra section */
            chunk_size = 0x20;
            header_offset = sb->xma_header_offset;
            if (header_offset == 0)
                header_offset = sb->extra_offset;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, sb->stream_size, sf_head, 1);

            vgmstream->codec_data = init_ffmpeg_header_offset(sf_data, buf, bytes, start_offset, sb->stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_ch(vgmstream, sf_data, start_offset, sb->stream_size, sb->channels, 0, 0);
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case FMT_OGG: {
            vgmstream->codec_data = init_ogg_vorbis(sf_data, start_offset, sb->stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        case FMT_CWAV:
            if (sb->channels > 1) goto fail; /* unknown layout */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            dsp_read_coefs_le(vgmstream,sf_data,start_offset + 0x7c, 0x40);
            start_offset += 0xe0; /* skip CWAV header */
            break;

        case FMT_APM:
            /* APM is a full format though most fields are repeated from .bnm
             * (info from https://github.com/Synthesis/ray2get)
             * 0x00(2): format tag (0x2000 for Ubisoft ADPCM)
             * 0x02(2): channels
             * 0x04(4): sample rate
             * 0x08(4): byte rate? PCM samples?
             * 0x0C(2): block align
             * 0x0E(2): bits per sample
             * 0x10(4): header size
             * 0x14(4): "vs12"
             * 0x18(4): file size
             * 0x1C(4): nibble size
             * 0x20(4): -1?
             * 0x24(4): 0?
             * 0x28(4): high/low nibble flag (when loaded in memory)
             * 0x2C(N): ADPCM info per channel, last to first
             * - 0x00(4): ADPCM hist
             * - 0x04(4): ADPCM step index
             * - 0x08(4): copy of ADPCM data (after interleave, ex. R from data + 0x01)
             * 0x60(4): "DATA"
             * 0x64(N): ADPCM data
             */

            vgmstream->coding_type = coding_DVI_IMA_int;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;

            /* read initial hist (last to first) */
            {
                int i;
                for (i = 0; i < sb->channels; i++) {
                    vgmstream->ch[i].adpcm_history1_32 = read_32bitLE(start_offset + 0x2c + 0x0c*(sb->channels - 1 - i) + 0x00, sf_data);
                    vgmstream->ch[i].adpcm_step_index  = read_32bitLE(start_offset + 0x2c + 0x0c*(sb->channels - 1 - i) + 0x04, sf_data);
                }
            }
            //todo supposedly APM IMA removes lower 3b after assigning step, but wave looks a bit off (Rayman 2 only?):
            // ...; step = adpcm_table[step_index]; delta = (step >> 3); step &= (~7); ...

            start_offset += 0x64; /* skip APM header (may be internal or external) */

            if (vgmstream->num_samples == 0) {
                vgmstream->num_samples       = ima_bytes_to_samples(sb->stream_size - 0x64, sb->channels);
                vgmstream->loop_end_sample   = vgmstream->num_samples;
            }
            break;

        case FMT_MPDX:
            /* a custom, chunked MPEG format (sigh)
             * 0x00: samples? (related to size)
             * 0x04: "2RUS" (apparently "1RUS" for mono files)
             * Rest is a MPEG-like sync but not an actual MPEG header? (DLLs do refer it as MPEG)
             * Files may have multiple "2RUS" or just a big one
             * A companion .csb has some not-too-useful info */
            goto fail;

        default:
            VGM_LOG("UBI SB: unknown codec\n");
            goto fail;
    }

    /* open the actual for decoding (sf_data can be an internal or external stream) */
    if ( !vgmstream_open_stream(vgmstream, sf_data, start_offset) )
        goto fail;
    return vgmstream;

fail:
    VGM_LOG("UBI SB: init vgmstream error\n");
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_sb_audio(ubi_sb_header* sb, STREAMFILE* sf_index, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;

    if (sb->is_dat)
        return init_vgmstream_ubi_dat_main(sb, sf_index, sf);

    /* open external stream if needed */
    if (sb->is_external) {
        sf_data = open_streamfile_by_filename(sf, sb->resource_name);
        if (sf_data == NULL) {
            /* play silence if external file is not found  */
            vgm_logi("UBI SB: external file '%s' not found (put together)\n", sb->resource_name);
            concatn(sizeof(sb->readable_name), sb->readable_name, " (missing)");
            sb->duration = 1.0f;
            return init_vgmstream_ubi_sb_silence(sb);
        }
    }
    else {
        sf_data = sf;
    }


    /* init actual VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_base(sb, sf_index, sf_data, sb->stream_offset);
    if (!vgmstream) goto fail;


    if (sf_data != sf) close_streamfile(sf_data);
    return vgmstream;

fail:
    VGM_LOG("UBI SB: init audio error\n");
    if (sf_data != sf) close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_sb_layer(ubi_sb_header* sb, STREAMFILE* sf_index, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* sf_data = NULL;
    size_t full_stream_size = sb->stream_size;
    int i, total_channels = 0;

    if (sb->is_ps2_old) {
        /* no blocked layout yet, just open it as a normal file */
        return init_vgmstream_ubi_sb_audio(sb, sf_index, sf);
    }

    /* open external stream if needed */
    if (sb->is_external) {
        sf_data = open_streamfile_by_filename(sf,sb->resource_name);
        if (sf_data == NULL) {
            /* play silence if external file is not found  */
            vgm_logi("UBI SB: external file '%s' not found (put together)\n", sb->resource_name);
            concatn(sizeof(sb->readable_name), sb->readable_name, " (missing)");
            sb->duration = 1.0f;
            return init_vgmstream_ubi_sb_silence(sb);
        }
    }
    else {
        sf_data = sf;
    }

    /* init layout */
    data = init_layout_layered(sb->layer_count);
    if (!data) goto fail;

    /* open all layers and mix */
    for (i = 0; i < sb->layer_count; i++) {
        /* prepare streamfile from a single layer section */
        temp_sf = setup_ubi_sb_streamfile(sf_data, sb->stream_offset, full_stream_size, i, sb->layer_count, sb->big_endian, sb->cfg.layer_hijack);
        if (!temp_sf) goto fail;

        sb->stream_size = get_streamfile_size(temp_sf);
        sb->channels = sb->layer_channels[i];
        total_channels += sb->layer_channels[i];

        /* build the layer VGMSTREAM (standard sb with custom streamfile) */
        data->layers[i] = init_vgmstream_ubi_sb_base(sb, sf_index, temp_sf, 0x00);
        if (!data->layers[i]) goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;


    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(total_channels, sb->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = sb->sample_rate;
    vgmstream->num_streams = sb->total_subsongs;
    vgmstream->stream_size = full_stream_size;

    vgmstream->num_samples = sb->num_samples;
    vgmstream->loop_start_sample = sb->loop_start;
    vgmstream->loop_end_sample = sb->num_samples;

    vgmstream->coding_type = data->layers[0]->coding_type;
    vgmstream->layout_type = layout_layered;
    vgmstream->layout_data = data;

    if (sf_data != sf) close_streamfile(sf_data);

    return vgmstream;
fail:
    VGM_LOG("UBI SB: init layer error\n");
    close_streamfile(temp_sf);
    if (sf_data != sf) close_streamfile(sf_data);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_layered(data);
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_sb_sequence(ubi_sb_header* sb, STREAMFILE* sf_index, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data = NULL;
    int i;
    STREAMFILE* sf_bank = sf_index;


    //todo optimization: open sf_data once / only if new name (doesn't change 99% of the time)

    /* init layout */
    data = init_layout_segmented(sb->sequence_count);
    if (!data) goto fail;

    sb->channels = 0;
    sb->num_samples = 0;

    /* open all segments and mix */
    for (i = 0; i < sb->sequence_count; i++) {
        ubi_sb_header temp_sb = {0};
        off_t entry_offset;
        int entry_index = sb->sequence_chain[i];


        /* bnm sequences may use to entries from other banks, do some voodoo */
        if (sb->has_numbered_banks) {
            /* see if *current* bank has changed (may use a different bank N times) */
            if (is_other_bank(sb, sf_bank, sb->sequence_banks[i])) {
                char bank_name[255];

                if (sf_bank != sf_index)
                    close_streamfile(sf_bank);

                get_ubi_bank_name(sb, sb->sequence_banks[i], bank_name);
                sf_bank = open_streamfile_by_filename(sf, bank_name);

                /* may be worth trying in localized folder? */
                //if (!sf_bank) {
                //    sprintf(bank_name, "English/Bnk_%i.bnm", sb->sequence_banks[i]);
                //    sf_bank = open_streamfile_by_filename(sf, bank_name);
                //}

                if (!sf_bank) {
                    VGM_LOG("UBI SB: sequence bank %i not found\n", sb->sequence_banks[i]);
                    goto fail;
                }

                //;VGM_LOG("UBI SB: opened %s\n", bank_name);
            }

            /* re-parse the thing */
            if (!parse_ubi_bank_header(sb, &temp_sb, sf_bank))
                goto fail;
            temp_sb.total_subsongs = 1; /* eh... just to keep parse_header happy */
        }
        else {
            temp_sb = *sb;  /* memcpy'ed */
        }

        ///* not detectable in .bnm */
        //if (entry_index > temp_sb.total_subsongs) {
        //    VGM_LOG("UBI SB: wrong sequence entry %i bank index %i (max: %i)\n", i, entry_index, temp_sb.total_subsongs);
        //    goto fail;
        //}

        /* parse expected entry */
        entry_offset = temp_sb.section2_offset + temp_sb.cfg.section2_entry_size * entry_index;
        if (!parse_header(&temp_sb, sf_bank, entry_offset, entry_index))
            goto fail;

        if (temp_sb.type == UBI_NONE || temp_sb.type == UBI_SEQUENCE) {
            VGM_LOG("UBI SB: unexpected sequence %i entry type %x at %x\n", i, temp_sb.type, (uint32_t)entry_offset);
            goto fail; /* not seen, technically ok but too much recursiveness? */
        }

        /* build the layer VGMSTREAM (current sb entry config) */
        data->segments[i] = init_vgmstream_ubi_sb_header(&temp_sb, sf_bank, sf);
        if (!data->segments[i]) goto fail;

        if (i == sb->sequence_loop)
            sb->loop_start = sb->num_samples;
        sb->num_samples += data->segments[i]->num_samples;

        /* save current (silences don't have values, so this ensures they know later, when memcpy'ed) */
        sb->channels = temp_sb.channels;
        sb->sample_rate = temp_sb.sample_rate;
    }

    if (sf_bank != sf_index)
        close_streamfile(sf_bank);

    if (!setup_layout_segmented(data))
        goto fail;

    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(data->output_channels, !sb->sequence_single);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_streams = sb->total_subsongs;
    //vgmstream->stream_size = sb->stream_size; /* auto when getting avg br */

    vgmstream->num_samples = sb->num_samples;
    vgmstream->loop_start_sample = sb->loop_start;
    vgmstream->loop_end_sample = sb->num_samples;

    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;
fail:
    VGM_LOG("UBI SB: init sequence error\n");
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_segmented(data);
    if (sf_bank != sf_index)
        close_streamfile(sf_bank);
    return NULL;
}


static VGMSTREAM* init_vgmstream_ubi_sb_silence(ubi_sb_header* sb) {
    VGMSTREAM* vgmstream = NULL;
    int channels, sample_rate;
    int32_t num_samples;

    /* by default silences don't have settings */
    channels = sb->channels;
    if (channels == 0)
        channels = 2;
    sample_rate = sb->sample_rate;
    if (sample_rate == 0)
        sample_rate = 48000;
    num_samples = (int)(sb->duration * sample_rate);


    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_silence(channels, sample_rate, num_samples);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->num_streams = sb->total_subsongs;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return vgmstream;
}


static VGMSTREAM* init_vgmstream_ubi_sb_header(ubi_sb_header* sb, STREAMFILE* sf_index, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    if (sb->total_subsongs == 0) {
        vgm_logi("UBI SB: bank has no subsongs (ignore)\n");
        goto fail;
    }

    ;VGM_LOG("UBI SB: target at %x + %x, extra=%x, name=%s, sb=%i, t=%i\n",
        (uint32_t)sb->header_offset, sb->cfg.section2_entry_size, (uint32_t)sb->extra_offset, sb->resource_name, sb->subblock_id, sb->stream_type);
    ;VGM_LOG("UBI SB: stream offset=%x, size=%x, name=%s\n", (uint32_t)sb->stream_offset, sb->stream_size, sb->is_external ? sb->resource_name : "internal" );

    switch(sb->type) {
        case UBI_AUDIO:
            vgmstream = init_vgmstream_ubi_sb_audio(sb, sf_index, sf);
            break;

        case UBI_LAYER:
            vgmstream = init_vgmstream_ubi_sb_layer(sb, sf_index, sf);
            break;

        case UBI_SEQUENCE:
            vgmstream = init_vgmstream_ubi_sb_sequence(sb, sf_index, sf);
            break;

        case UBI_SILENCE:
            vgmstream = init_vgmstream_ubi_sb_silence(sb);
            break;

        case UBI_NONE:
        default:
            VGM_LOG("UBI SB: subsong not found/parsed\n");
            goto fail;
    }

    if (!vgmstream) goto fail;

    strcpy(vgmstream->stream_name, sb->readable_name);
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************************************************* */

static void build_readable_name(char * buf, size_t buf_size, ubi_sb_header* sb) {
    const char *grp_name;
    const char *res_name;
    uint32_t id;
    uint32_t type;
    int index;

    /* config */
    if (sb->is_map) {
        grp_name = sb->map_name;
    }
    else if (sb->is_bnm || sb->is_ps2_bnm) {
        if (sb->sequence_multibank)
            grp_name = "bnm-multi";
        else
            grp_name = "bnm";
    }
    else if (sb->is_dat) {
        if (sb->sequence_multibank)
            grp_name = "dat-multi";
        else
            grp_name = "dat";
    }
    else if (sb->is_blk) {
        grp_name = "blk";
    }
    else {
        grp_name = "bank";
    }
    id = sb->header_id;
    type = sb->header_type;
    if (sb->is_map)
        index = sb->header_index; //sb->bank_subsongs;
    else
        index = sb->header_index; //-1

    if (sb->type == UBI_SEQUENCE) {
        if (sb->sequence_single) {
            if (sb->sequence_count == 1)
                res_name = "single";
            else
                res_name = "multi";
        }
        else {
            if (sb->sequence_count == 1)
                res_name = "single-loop";
            else
                res_name = (sb->sequence_loop == 0) ? "multi-loop" : "intro-loop";
        }
    }
    else {
        if (sb->is_external || sb->cfg.audio_has_internal_names)
            res_name = sb->resource_name;
        else
            res_name = NULL;
    }

    /* maps can contain +10000 subsongs, we need something helpful
     * (best done right after subsong detection, since some sequence re-parse types) */
    if (res_name && res_name[0]) {
        if (index >= 0)
            snprintf(buf,buf_size, "%s/%04d/%02x-%08x/%s", grp_name, index, type, id, res_name);
        else
            snprintf(buf,buf_size, "%s/%02x-%08x/%s", grp_name, type, id, res_name);
    }
    else {
        if (index >= 0)
            snprintf(buf,buf_size, "%s/%04d/%02x-%08x", grp_name, index, type, id);
        else
            snprintf(buf,buf_size, "%s/%02x-%08x", grp_name, type, id);
    }
}

static int parse_type_audio_ps2_bnm(ubi_sb_header *sb, off_t offset, STREAMFILE *sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE *) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t(*read_16bit)(off_t, STREAMFILE *) = sb->big_endian ? read_16bitBE : read_16bitLE;

    sb->stream_size     = read_32bit(offset + sb->cfg.audio_stream_size, sf);
    sb->stream_offset   = read_32bit(offset + sb->cfg.audio_stream_offset, sf);
    sb->channels        = read_8bit(offset + sb->cfg.audio_channels, sf);
    sb->sample_rate     = (uint16_t)read_16bit(offset + sb->cfg.audio_sample_rate, sf);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    sb->is_streamed     = read_32bit(offset + sb->cfg.audio_streamed_flag, sf) & sb->cfg.audio_streamed_and;
    sb->is_cd_streamed  = read_32bit(offset + sb->cfg.audio_cd_streamed_flag, sf) & sb->cfg.audio_cd_streamed_and;
    sb->loop_flag       = read_32bit(offset + sb->cfg.audio_loop_flag, sf) & sb->cfg.audio_loop_and;

    sb->num_samples = 0; /* calculate from size */

    if (!sb->is_cd_streamed) {
        sb->stream_size *= sb->channels;
    }

    if (sb->is_streamed) {
        if (sb->is_cd_streamed) {
            /* streamed from CD */
            sprintf(sb->resource_name, "BNK_%d.VSC", sb->bank_number);
        } else {
            /* streamed from RAM */
            sprintf(sb->resource_name, "BNK_%d.VSB", sb->bank_number);
        }
    } else {
        /* loaded fully into SPU memory */
        sprintf(sb->resource_name, "BNK_%d.VB", sb->bank_number);
    }

    sb->is_external = 1;

    return 1;
fail:
    return 0;
}

static uint32_t ubi_ps2_pitch_to_freq(uint32_t pitch) {
    /* old PS2 games store sample rate in a weird range of 0-65536 remapped from 0-48000 */
    /* strangely, audio res type does have sample rate value but it's unused */
    double sample_rate = (((double)pitch / 65536) * 48000);
    return (uint32_t)ceil(sample_rate);
}

static int parse_type_audio_ps2_old(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t pitch;
    uint32_t test_sample_rate;
    int is_stereo;

    sb->stream_size     = read_32bit(offset + sb->cfg.audio_stream_size, sf);
    sb->stream_offset   = read_32bit(offset + sb->cfg.audio_stream_offset, sf);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    pitch               = read_32bit(offset + sb->cfg.audio_pitch, sf);
    test_sample_rate    = read_32bit(offset + sb->cfg.audio_sample_rate, sf);
    sb->sample_rate     = ubi_ps2_pitch_to_freq(pitch);
    VGM_ASSERT(sb->sample_rate != test_sample_rate, "UBI SB: Converted PS2 sample rate mismatch (%d = %d vs %d)\n", pitch, sb->sample_rate, test_sample_rate);

    sb->is_streamed     = read_32bit(offset + sb->cfg.audio_streamed_flag, sf) & sb->cfg.audio_streamed_and;
    sb->loop_flag       = read_32bit(offset + sb->cfg.audio_loop_flag, sf) & sb->cfg.audio_loop_and;
    sb->is_localized    = read_32bit(offset + sb->cfg.audio_loc_flag, sf) & sb->cfg.audio_loc_and;
    is_stereo           = read_32bit(offset + sb->cfg.audio_stereo_flag, sf) & sb->cfg.audio_stereo_and;

    sb->num_samples = 0; /* calculate from size */
    sb->channels = is_stereo ? 2 : 1;
    sb->stream_size *= sb->channels;
    sb->subblock_id = 0;

    /* filenames are hardcoded */
    if (sb->is_blk) {
        blk_get_resource_name(sb);
        sb->is_external = 1;
    } else if (sb->is_streamed) {
        strcpy(sb->resource_name, sb->is_localized ? "STRM.LM1" : "STRM.SM1");
        sb->is_external = 1;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_layer_ps2_old(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t(*read_32bit)(off_t, STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t pitch;

    /* much simpler than later iteration */
    sb->layer_count     = read_32bit(offset + sb->cfg.layer_layer_count, sf);
    sb->stream_size     = read_32bit(offset + sb->cfg.audio_stream_size, sf);
    sb->stream_offset   = read_32bit(offset + sb->cfg.audio_stream_offset, sf);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    if (sb->layer_count > SB_MAX_LAYER_COUNT) {
        VGM_LOG("UBI SB: incorrect layer count\n");
        goto fail;
    }

    pitch               = read_32bit(offset + sb->cfg.layer_pitch, sf);
    sb->sample_rate     = ubi_ps2_pitch_to_freq(pitch);
    sb->is_localized    = read_32bit(offset + sb->cfg.layer_loc_flag, sf) & sb->cfg.layer_loc_and;

    sb->num_samples = 0; /* calculate from size */
    sb->channels = sb->layer_count * 2; /* layers are always stereo */
    sb->stream_size *= sb->channels;

    /* filenames are hardcoded */
    if (sb->is_blk) {
        blk_get_resource_name(sb);
        sb->is_external = 1;
    } else if (sb->is_streamed) {
        strcpy(sb->resource_name, sb->is_localized ? "STRM.LM1" : "STRM.SM1");
        sb->is_external = 1;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_audio(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;

    /* audio header */
    sb->type = UBI_AUDIO;

    if (sb->is_ps2_bnm)
        return parse_type_audio_ps2_bnm(sb, offset, sf);

    if (sb->is_ps2_old)
        return parse_type_audio_ps2_old(sb, offset, sf);

    sb->extra_offset    = read_32bit(offset + sb->cfg.audio_extra_offset, sf) + sb->sectionX_offset;
    sb->stream_size     = read_32bit(offset + sb->cfg.audio_stream_size, sf);
    sb->stream_offset   = read_32bit(offset + sb->cfg.audio_stream_offset, sf);
    sb->channels        = (sb->cfg.audio_channels % 4) ? /* non-aligned offset is always 16b */
                (uint16_t)read_16bit(offset + sb->cfg.audio_channels, sf) :
                (uint32_t)read_32bit(offset + sb->cfg.audio_channels, sf);
    sb->sample_rate     = read_32bit(offset + sb->cfg.audio_sample_rate, sf);
    sb->stream_type     = read_32bit(offset + sb->cfg.audio_stream_type, sf);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    sb->is_streamed = read_32bit(offset + sb->cfg.audio_streamed_flag, sf) & sb->cfg.audio_streamed_and;
    sb->is_external = sb->is_streamed;

    if (sb->cfg.audio_internal_flag && !sb->is_streamed) {
        /* RAM sounds are not always internal in early versions [Donald Duck Demo (PC)] */
        sb->is_external = (int)!(read_32bit(offset + sb->cfg.audio_internal_flag, sf));
    }

    if (sb->cfg.audio_software_flag && sb->cfg.audio_software_and) {
        /* software decoded and hardware decoded sounds are stored in separate subblocks */
        int software_flag = read_32bit(offset + sb->cfg.audio_software_flag, sf) & sb->cfg.audio_software_and;
        sb->subblock_id = (!software_flag) ? 0 : 1;

        if (sb->platform == UBI_PS2) {
            /* flag appears to mean "load into IOP memory instead of SPU" */
            int hwmodule_flag = read_32bit(offset + sb->cfg.audio_hwmodule_flag, sf) & sb->cfg.audio_hwmodule_and;
            sb->subblock_id = (!software_flag) ? ((!hwmodule_flag) ? 0 : 3) : 1;
        }

        /* PC can have subblock 2 based on two fields near the end but it wasn't seen so far */

        /* stream_type field is not used for HW sounds and may contain garbage
         * except for PS3 and new PSP which have two hardware codecs (PSX and AT3) */
        if (!software_flag && sb->platform != UBI_PS3 && !(sb->platform == UBI_PSP && !sb->is_psp_old))
            sb->stream_type = 0x00;
    } else {
        sb->subblock_id = (sb->stream_type == 0x01) ? 0 : 1;
    }

    if (sb->cfg.has_rs_files && !sb->is_external) {
        /* found in Splinter Cell: Pandora Tomorrow (PS2) */
        sb->is_ram_streamed = read_32bit(offset + sb->cfg.audio_ram_streamed_flag, sf) & sb->cfg.audio_ram_streamed_and;
        sb->is_external = sb->is_ram_streamed;
    }

    sb->loop_flag = read_32bit(offset + sb->cfg.audio_loop_flag, sf) & sb->cfg.audio_loop_and;

    if (sb->loop_flag) {
        sb->loop_start  = read_32bit(offset + sb->cfg.audio_num_samples, sf);
        sb->num_samples = read_32bit(offset + sb->cfg.audio_num_samples2, sf) + sb->loop_start;

        if (sb->cfg.audio_num_samples == sb->cfg.audio_num_samples2) { /* early games just repeat and don't set loop start */
            sb->num_samples = sb->loop_start;
            sb->loop_start = 0;
        }
        /* Loop starts that aren't 0 do exist but are very rare (ex. Splinter Cell PC, Beowulf PSP sb 82, index 575).
         * Also rare are looping external streams, since it's normally done through sequences (ex. Surf's Up).
         * Loop end may be +1? (ex. Splinter Cell: Double Agent PS3 #14331). */
    } else {
        sb->num_samples = read_32bit(offset + sb->cfg.audio_num_samples, sf);
    }

    if (sb->cfg.resource_name_size > sizeof(sb->resource_name)) {
        goto fail;
    }

    /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
    if (sb->cfg.audio_stream_name) {
        if (sb->is_dat && !sb->is_external) {
            sb->subbank_index = read_8bit(offset + sb->cfg.audio_stream_name + 0x01, sf);
        } else if (sb->cfg.has_rs_files && sb->is_ram_streamed) {
            strcpy(sb->resource_name, "MAPS.RS1");
        } else if (sb->is_external || sb->cfg.audio_has_internal_names) {
            read_string(sb->resource_name, sb->cfg.resource_name_size, offset + sb->cfg.audio_stream_name, sf);
        }
    }
    else {
        sb->cfg.audio_stream_name = read_32bit(offset + sb->cfg.audio_extra_name, sf);
        if (sb->cfg.audio_stream_name != 0xFFFFFFFF)
            read_string(sb->resource_name, sb->cfg.resource_name_size, sb->sectionX_offset + sb->cfg.audio_stream_name, sf);
    }

    /* points at XMA1 header in the extra section (only for RAW_XMA1, ignored garbage otherwise) */
    if (sb->cfg.audio_xma_offset) {
        sb->xma_header_offset = read_32bit(offset + sb->cfg.audio_xma_offset, sf) + sb->sectionX_offset;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_sequence(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    off_t table_offset;
    int i;

    /* sequence chain */
    sb->type = UBI_SEQUENCE;
    if (sb->cfg.sequence_sequence_count == 0) {
        VGM_LOG("UBI SB: sequence not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    sb->extra_offset    = read_32bit(offset + sb->cfg.sequence_extra_offset, sf) + sb->sectionX_offset;
    sb->sequence_loop   = read_32bit(offset + sb->cfg.sequence_sequence_loop, sf);
    sb->sequence_single = read_32bit(offset + sb->cfg.sequence_sequence_single, sf);
    sb->sequence_count  = read_32bit(offset + sb->cfg.sequence_sequence_count, sf);

    if (sb->sequence_count > SB_MAX_CHAIN_COUNT) {
        VGM_LOG("UBI SB: incorrect sequence count %i vs %i\n", sb->sequence_count, SB_MAX_CHAIN_COUNT);
        goto fail;
    }

    /* get chain in extra table */
    table_offset = sb->extra_offset;
    for (i = 0; i < sb->sequence_count; i++) {
        uint32_t entry_number = (uint32_t)read_32bit(table_offset+sb->cfg.sequence_entry_number, sf);

        /* bnm sequences may refer to entries from different banks, whee */
        if (sb->has_numbered_banks) {
            int16_t bank_number = (entry_number >> 16) & 0xFFFF;
            entry_number        = (entry_number >> 00) & 0xFFFF;

            //;VGM_LOG("UBI SB: bnm sequence entry=%i, bank=%i at %lx\n", entry_number, bank_number, table_offset);
            sb->sequence_banks[i] = bank_number;

            /* info flag, does bank number point to another file? */
            if (!sb->sequence_multibank) {
                sb->sequence_multibank = is_other_bank(sb, sf, bank_number);
            }
        } else {
            entry_number = entry_number & 0x3FFFFFFF;
            if (entry_number > sb->section2_num) {
                VGM_LOG("UBI SB: chain with wrong entry %i vs %i at %x\n", entry_number, sb->section2_num, (uint32_t)sb->extra_offset);
                goto fail;
            }
        }

        /* some sequences have an upper bit (2 bits in Donald Duck voices) for some reason */
        //;VGM_ASSERT_ONCE(entry_number & 0xC0000000, "UBI SB: sequence bit entry found at %x\n", (uint32_t)sb->extra_offset);

        sb->sequence_chain[i] = entry_number;

        table_offset += sb->cfg.sequence_entry_size;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_layer(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;
    off_t table_offset;
    int i;

    /* layer header */
    sb->type = UBI_LAYER;
    if (sb->cfg.layer_layer_count == 0) {
        VGM_LOG("UBI SB: layers not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    /* all layers seem streamed */
    sb->is_streamed = 1;

    if (sb->is_ps2_old)
        return parse_type_layer_ps2_old(sb, offset, sf);

    sb->extra_offset    = read_32bit(offset + sb->cfg.layer_extra_offset, sf) + sb->sectionX_offset;
    sb->layer_count     = read_32bit(offset + sb->cfg.layer_layer_count, sf);
    sb->stream_size     = read_32bit(offset + sb->cfg.layer_stream_size, sf);
    sb->stream_offset   = read_32bit(offset + sb->cfg.layer_stream_offset, sf);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    if (sb->layer_count > SB_MAX_LAYER_COUNT) {
        VGM_LOG("UBI SB: incorrect layer count\n");
        goto fail;
    }

    sb->is_external = sb->is_streamed;

    /* get 1st layer header in extra table and validate all headers match */
    table_offset = sb->extra_offset;
    //sb->channels        = (sb->cfg.layer_channels % 4) ? /* non-aligned offset is always 16b */
    //            (uint16_t)read_16bit(table_offset + sb->cfg.layer_channels, sf) :
    //            (uint32_t)read_32bit(table_offset + sb->cfg.layer_channels, sf);
    sb->sample_rate = read_32bit(table_offset + sb->cfg.layer_sample_rate, sf);
    sb->stream_type = read_32bit(table_offset + sb->cfg.layer_stream_type, sf);
    sb->num_samples = read_32bit(table_offset + sb->cfg.layer_num_samples, sf);

    for (i = 0; i < sb->layer_count; i++) {
        int channels = (sb->cfg.layer_channels % 4) ? /* non-aligned offset is always 16b */
            (uint16_t)read_16bit(table_offset + sb->cfg.layer_channels, sf) :
            (uint32_t)read_32bit(table_offset + sb->cfg.layer_channels, sf);
        int sample_rate = read_32bit(table_offset + sb->cfg.layer_sample_rate, sf);
        int stream_type = read_32bit(table_offset + sb->cfg.layer_stream_type, sf);
        int num_samples = read_32bit(table_offset + sb->cfg.layer_num_samples, sf);

        if (sb->sample_rate != sample_rate || sb->stream_type != stream_type) {
            VGM_LOG("UBI SB: %i layer headers don't match at %x > %x\n", sb->layer_count, (uint32_t)offset, (uint32_t)table_offset);
            /* Layers of different rates happens sometimes. From decompilations, first layer's sample rate
             * looks used as main, though lower sample rate layer only seem to appear to after first. */
            if (!sb->cfg.ignore_layer_error)
                goto fail;
        }

        /* uncommonly channels may vary per layer [Brothers in Arms 2 (PS2) ex. MP_B01_NL.SB1] */
        sb->layer_channels[i] = channels;

        /* can be +-1 */
        if (sb->num_samples != num_samples && sb->num_samples + 1 == num_samples) {
            sb->num_samples -= 1;
        }

        table_offset += sb->cfg.layer_entry_size;
    }

    /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
    if (sb->cfg.layer_stream_name) {
        read_string(sb->resource_name, sb->cfg.resource_name_size, offset + sb->cfg.layer_stream_name, sf);
    } else if (sb->cfg.layer_extra_name) {
        sb->cfg.layer_stream_name = read_32bit(offset + sb->cfg.layer_extra_name, sf);
        if (sb->cfg.layer_stream_name != 0xFFFFFFFF)
            read_string(sb->resource_name, sb->cfg.resource_name_size, sb->sectionX_offset + sb->cfg.layer_stream_name, sf);
    }

    /* layers seem to include XMA header */

    return 1;
fail:
    return 0;
}

static int parse_type_silence(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    float (*read_f32)(off_t,STREAMFILE*) = sb->big_endian ? read_f32be : read_f32le;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* silence header */
    sb->type = UBI_SILENCE;
    if (sb->cfg.silence_duration_int == 0 && sb->cfg.silence_duration_float == 0) {
        VGM_LOG("UBI SB: silence duration not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    if (sb->cfg.silence_duration_int) {
        uint32_t duration_int = (uint32_t)read_32bit(offset + sb->cfg.silence_duration_int, sf);
        sb->duration = (float)duration_int / 65536.0f; /* 65536.0 is common so probably means 1.0 */
    }
    else if (sb->cfg.silence_duration_float) {
        sb->duration = read_f32(offset + sb->cfg.silence_duration_float, sf);
    }

    return 1;
fail:
    return 0;
}

// todo improve, only used in bnm sequences as sequence end (and may point to another bnm)
static int parse_type_random(ubi_sb_header* sb, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    off_t sb_extra_offset, table_offset;
    int i, sb_sequence_count;

    /* sequence chain */
    if (sb->cfg.random_entry_size == 0) {
        VGM_LOG("UBI SB: random entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    sb_extra_offset    = read_32bit(offset + sb->cfg.random_extra_offset, sf) + sb->sectionX_offset;
    sb_sequence_count  = read_32bit(offset + sb->cfg.random_sequence_count, sf);


    /* get chain in extra table */
    table_offset = sb_extra_offset;
    for (i = 0; i < sb_sequence_count; i++) {
        uint32_t entry_number = (uint32_t)read_32bit(table_offset+0x00, sf);
        //uint32_t entry_chance = (uint32_t)read_32bit(table_offset+0x04, sf);

        if (sb->has_numbered_banks) {
            int16_t bank_number = (entry_number >> 16) & 0xFFFF;
            entry_number        = (entry_number >> 00) & 0xFFFF;

            ;VGM_LOG("UBI SB: bnm sequence entry=%i, bank=%i\n", entry_number, bank_number);
            //sb->sequence_banks[i] = bank_number;

            /* not seen */
            if (is_other_bank(sb, sf, bank_number)) {
                VGM_LOG("UBI SB: random in other bank\n");
                goto fail;
            }
        } else {
            entry_number = entry_number & 0x3FFFFFFF;
            if (entry_number > sb->section2_num) {
                VGM_LOG("UBI SB: random with wrong entry %i vs %i at %x\n", entry_number, sb->section2_num, (uint32_t)sb->extra_offset);
                goto fail;
            }
        }

        //todo make rand or stuff (old chance: int from 0 to 0x10000, new: float from 0.0 to 1.0)
        { //if (entry_chance == ...)
            off_t entry_offset = sb->section2_offset + sb->cfg.section2_entry_size * entry_number;
            return parse_type_audio(sb, entry_offset, sf);
        }

        table_offset += sb->cfg.random_entry_size;
    }

    return 1;
fail:
    return 0;
}

static int set_hardware_codec_for_platform(ubi_sb_header *sb) {
    switch (sb->platform) {
        case UBI_PC:
            sb->codec = RAW_PCM;
            break;
        case UBI_PS2:
            sb->codec = RAW_PSX;
            break;
        case UBI_PSP:
            if (sb->is_psp_old)
                sb->codec = FMT_VAG;
            else
                sb->codec = RAW_PSX;
            break;
        case UBI_XBOX:
            sb->codec = RAW_XBOX;
            break;
        case UBI_GC:
        case UBI_WII:
            sb->codec = RAW_DSP;
            break;
        case UBI_X360:
            sb->codec = RAW_XMA1;
            break;
        case UBI_3DS:
            sb->codec = FMT_CWAV;
            break;
        default:
            VGM_LOG("UBI SB: unknown hardware codec\n");
            return 0;
    }

    return 1;
}

/* find actual codec from type (as different games' stream_type can overlap) */
static int parse_stream_codec(ubi_sb_header* sb) {

    if (sb->type != UBI_AUDIO && sb->type != UBI_LAYER)
        return 1;

    if (sb->is_dat) {
        /* handled separately */
        return 1;
    }

    if (sb->is_ps2_bnm || sb->is_ps2_old) {
        /* early PS2 games don't support different codecs, it's always PSX ADPCM */
        sb->codec = RAW_PSX;
        return 1;
    }

    /* guess codec */
    if (sb->is_bnm || sb->version < 0x00000007) { /* bnm is ~v0 but some games have wonky versions */
        switch (sb->stream_type) {
            case 0x01:
                if (sb->is_streamed)
                    sb->codec = RAW_PCM;
                else if (!set_hardware_codec_for_platform(sb))
                    goto fail;
                break;

            case 0x02:
                sb->codec = FMT_MPDX;
                break;

            case 0x04:
                sb->codec = FMT_APM;
                break;

            case 0x06:
                sb->codec = UBI_ADPCM;
                break;
#if 0
            case 0x07:
                sb->codec = FMT_PFK; /* not seen yet, some MPEG based codec, referred to as "PFK" in the code */
                break;
#endif
            case 0x08:
                sb->codec = UBI_IMA; /* Ubi IMA v2/v3 */
                break;

            default:
                VGM_LOG("UBI SB: unknown stream_type %02x for version %08x\n", sb->stream_type, sb->version);
                goto fail;
        }
    } else if (sb->version < 0x000A0000) {
        switch (sb->stream_type) {
            case 0x01:
                if (sb->is_streamed)
                    sb->codec = RAW_PCM;
                else if (!set_hardware_codec_for_platform(sb))
                    goto fail;
                break;

            case 0x02:
                sb->codec = UBI_ADPCM;
                break;
#if 0
            case 0x03:
                sb->codec = FMT_PFK; /* not seen yet, some MPEG based codec, referred to as "PFK" in the code */
                break;
#endif
            case 0x04:
                sb->codec = UBI_IMA; /* Ubi IMA v2/v3 */
                break;
#if 0
            case 0x05:
                sb->codec = FMT_OGG; /* not seen yet */
                break;
#endif
            default:
                VGM_LOG("UBI SB: Unknown stream_type %02x for version %08x\n", sb->stream_type, sb->version);
                goto fail;
        }
    } else {
        switch (sb->stream_type) {
            case 0x00:
                if (!set_hardware_codec_for_platform(sb))
                    goto fail;
                break;

            case 0x01:
                sb->codec = RAW_PCM; /* uncommon, ex. Wii/PSP/3DS */
                break;

            case 0x02:
                switch (sb->platform) {
                    case UBI_PC:
                    case UBI_XBOX:
                        sb->codec = UBI_ADPCM;
                        break;
                    case UBI_PS3:
                        sb->codec = RAW_PSX; /* PS3 */
                        break;
                    case UBI_PSP: /* Splinter Cell: Essentials (PSP) */
                        sb->codec = UBI_IMA_SCE;
                        break;
                    default:
                        VGM_LOG("UBI SB: unknown codec for stream_type %02x\n", sb->stream_type);
                        goto fail;
                }
                break;

            case 0x03:
                sb->codec = UBI_IMA; /* Ubi IMA v3+ (versions handled in decoder) */
                break;

            case 0x04:
                sb->codec = FMT_OGG; /* later PC games */
                break;

            case 0x05:
                switch (sb->platform) {
                    case UBI_X360:
                        sb->codec = FMT_XMA1;
                        break;
                    case UBI_PS3:
                    case UBI_PSP:
                        sb->codec = FMT_AT3;
                        break;
                    default:
                        VGM_LOG("UBI SB: unknown codec for stream_type %02x\n", sb->stream_type);
                        goto fail;
                }
                break;

            case 0x06:
                sb->codec = RAW_PSX; /* later PSP and PS3(?) games */
                break;

            case 0x07:
                sb->codec = RAW_AT3; /* PS3 */
                break;

            case 0x08:
                sb->codec = FMT_AT3; /* PS3 */
                break;

            default:
                VGM_LOG("UBI SB: Unknown stream_type %02x\n", sb->stream_type);
                goto fail;
        }
    }

    return 1;
fail:
    return 0;
}

/* find actual stream offset in section3 */
static int parse_offsets(ubi_sb_header* sb, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t i, j, k;

    if (sb->type != UBI_AUDIO && sb->type != UBI_LAYER)
        return 1;

    if (sb->is_bnm)
        return bnm_parse_offsets(sb, sf);

    /* handled separately */
    if (sb->is_dat)
        return 1;

    if (sb->is_ps2_bnm) {
        if (sb->is_cd_streamed) {
            /* offsets for CD streams are stored in sectors */
            sb->stream_offset *= 0x800;
        }
        return 1;
    }

    if (sb->is_blk)
        return blk_parse_offsets(sb);

    VGM_ASSERT(!sb->is_map && sb->section3_num > 2, "UBI SB: section3 > 2 found\n");

    /* Internal sounds are split into subblocks, with their offsets being relative to subblock start.
     * A table contains sizes of each subblock, so we adjust offsets based on the subblock ID of our sound.
     * Headers normally only use 0 or 1, and section3 may only define id1 (which the internal sound would use).
     * May exist even for external streams only, and they often use id 1 too. */

    if (sb->is_map) {
        /* maps store internal sounds offsets in a separate subtable, find the matching entry
         * each sec3 entry consists of the header and two tables
         * 0x00: some ID? (always -1 for the first entry)
         * 0x04: table 1 offset
         * 0x08: table 1 entries
         * 0x0c: table 2 offset
         * 0x10: table 2 entries
         * table 1 - for each entry:
         *   0x00: sec2 entry index
         *   0x04: sound offset
         * table 2 - for each entry:
         *   0x00 - subblock ID
         *   0x04 - size with padding included
         *   0x08 - size without padding
         *   0x0c - absolute subblock offset
         */

        if (sb->is_external && !sb->is_ram_streamed)
            return 1;

        for (i = 0; i < sb->section3_num; i++) {
            off_t offset = sb->section3_offset + 0x14 * i;
            off_t table_offset  = read_32bit(offset + 0x04, sf) + sb->section3_offset;
            uint32_t table_num  = read_32bit(offset + 0x08, sf);
            off_t table2_offset = read_32bit(offset + 0x0c, sf) + sb->section3_offset;
            uint32_t table2_num = read_32bit(offset + 0x10, sf);

            for (j = 0; j < table_num; j++) {
                int index = read_32bit(table_offset + 0x08 * j + 0x00, sf) & 0x3FFFFFFF;

                if (index == sb->header_index) {
                    sb->stream_offset = read_32bit(table_offset + 0x08 * j + 0x04, sf);
                    if (sb->is_ram_streamed)
                        break;

                    for (k = 0; k < table2_num; k++) {
                        uint32_t id = read_32bit(table2_offset + 0x10 * k + 0x00, sf);

                        if (id == sb->subblock_id) {
                            sb->stream_offset += read_32bit(table2_offset + 0x10 * k + 0x0c, sf);
                            break;
                        }
                    }

                    if (k == table2_num) {
                        VGM_LOG("UBI SM: Failed to find subblock %d in map %s\n", sb->subblock_id, sb->map_name);
                        goto fail;
                    }
                    break;
                }
            }

            if (sb->stream_offset)
                break;
        }

        if (sb->stream_offset == 0) {
            VGM_LOG("UBI SM: Failed to find offset for resource %d in subblock %d in map %s\n", sb->header_index, sb->subblock_id, sb->map_name);
            goto fail;
        }
    } else {
        /* banks store internal sounds after all headers and adjusted by the subblock table, find the matching entry */
        off_t sounds_offset;

        if (sb->is_external)
            return 1;

        sounds_offset = sb->section3_offset + sb->cfg.section3_entry_size*sb->section3_num;
        if (sb->cfg.is_padded_sounds_offset)
            sounds_offset = align_size_to_block(sounds_offset, 0x10);
        sb->stream_offset = sounds_offset + sb->stream_offset;

        for (i = 0; i < sb->section3_num; i++) {
            off_t offset = sb->section3_offset + sb->cfg.section3_entry_size * i;

            /* table has unordered ids+size, so if our id doesn't match current data offset must be beyond */
            if (read_32bit(offset + 0x00, sf) == sb->subblock_id)
                break;
            sb->stream_offset += read_32bit(offset + 0x04, sf);
        }

        if (i == sb->section3_num) {
            VGM_LOG("UBI SB: Failed to find subblock %d\n", sb->subblock_id);
            goto fail;
        }
    }

    return 1;
fail:
    return 0;
}

/* parse a single known header resource at offset (see config_sb for info) */
static int parse_header(ubi_sb_header* sb, STREAMFILE* sf, off_t offset, int index) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    sb->header_index    = index;
    sb->header_offset   = offset;

    sb->header_id       = read_32bit(offset + 0x00, sf);
    sb->header_type     = read_32bit(offset + 0x04, sf);

    switch(sb->header_type) {
        case 0x01:
            if (!parse_type_audio(sb, offset, sf))
                goto fail;
            break;
        case 0x05:
        case 0x0b:
        case 0x0c:
            if (!parse_type_sequence(sb, offset, sf))
                goto fail;
            break;
        case 0x06:
        case 0x0d:
            if (!parse_type_layer(sb, offset, sf))
                goto fail;
            break;
        case 0x08:
        case 0x0f:
            if (!parse_type_silence(sb, offset, sf))
                goto fail;
            break;
        case 0x0a:
            if (!parse_type_random(sb, offset, sf))
                goto fail;
            break;
        case 0x00:
            if (sb->is_dat) {
                /* weird dummy entries in Donald Duck: Goin' Quackers (DC) */
                sb->type = UBI_SILENCE;
                sb->duration = 1.0f;
                break;
            }
        default:
            VGM_LOG("UBI SB: unknown header type %x at %x\n", sb->header_type, (uint32_t)offset);
            goto fail;
    }

    if (!parse_stream_codec(sb))
        goto fail;

    if (!parse_offsets(sb, sf))
        goto fail;

    return 1;
fail:
    return 0;
}

/* parse a bank and its possible audio headers */
static int parse_sb(ubi_sb_header* sb, STREAMFILE* sf, int target_subsong) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int i;

    //;VGM_LOG("UBI SB: s1=%x (%x*%x), s2=%x (%x*%x), sX=%x (%x), s3=%x (%x*%x)\n",
    //        sb->section1_offset,sb->cfg.section1_entry_size,sb->section1_num,sb->section2_offset,sb->cfg.section2_entry_size,sb->section2_num,
    //        sb->sectionX_offset,sb->sectionX_size,sb->section3_offset,sb->cfg.section3_entry_size,sb->section3_num);

    /* find target subsong info in section2 and keeps counting */
    sb->bank_subsongs = 0;
    for (i = 0; i < sb->section2_num; i++) {
        off_t offset = sb->section2_offset + sb->cfg.section2_entry_size*i;
        uint32_t header_type;

      /*header_id =*/ read_32bit(offset + 0x00, sf); /* forces buffer read */
        header_type = read_32bit(offset + 0x04, sf);

        if (header_type >= 0x10) {
            VGM_LOG("UBI SB: unknown type %x at %x\n", header_type, (uint32_t)offset);
            goto fail;
        }

        sb->types[header_type]++;
        if (!sb->allowed_types[header_type])
            continue;

        sb->bank_subsongs++;
        sb->total_subsongs++;
        if (sb->total_subsongs != target_subsong)
            continue;

        if (!parse_header(sb, sf, offset, i))
            goto fail;

        build_readable_name(sb->readable_name, sizeof(sb->readable_name), sb);
    }

    /* either found target subsong or it's in another bank (in case of maps), both handled externally */

    //;VGM_LOG("UBI SB: types "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(sb->types[i],"%02x=%i ",i,sb->types[i]); }} VGM_LOG("\n");

    return 1;
fail:
    return 0;
}

/* ************************************************************************* */

static int config_sb_platform(ubi_sb_header* sb, STREAMFILE* sf) {
    char filename[PATH_LIMIT];
    int filename_len;
    char platform_char;
    uint32_t version;

    /* to find out hijacking (LE) platforms */
    version = read_32bitLE(0x00, sf);

    /* get X from .sbX/smX/lmX */
    get_streamfile_name(sf,filename,sizeof(filename));
    filename_len = strlen(filename);
    platform_char = filename[filename_len - 1];

    switch(platform_char) {
        case '0':
            sb->platform = UBI_PC;
            break;
        case '1':
            sb->platform = UBI_PS2;
            break;
        case '2':
            sb->platform = UBI_XBOX;
            break;
        case '3':
            sb->platform = UBI_GC;
            break;
        case '4':
            switch(version) { /* early PSP clashes with X360 */
                case 0x0012000C: /* multiple games use this ID and all are sb4/sm4 */
                    sb->platform = UBI_PSP;
                    sb->is_psp_old = 1;
                    break;
                default:
                    sb->platform = UBI_X360;
                    break;
            }
            break;
        case '5':
            switch(version) { /* 3DS could be sb8/sm8 but somehow hijacks extension */
                case 0x00130001: /* Splinter Cell 3DS (2011) */
                    sb->platform = UBI_3DS;
                    break;
                default:
                    sb->platform = UBI_PSP;
                    break;
            }
            break;
        case '6':
            sb->platform = UBI_PS3;
            break;
        case '7':
            sb->platform = UBI_WII;
            break;
        default:
            goto fail;
    }

    sb->big_endian =
            sb->platform == UBI_GC ||
            sb->platform == UBI_PS3 ||
            sb->platform == UBI_X360 ||
            sb->platform == UBI_WII;

    return 1;
fail:
    return 0;
}


static void config_sb_entry(ubi_sb_header* sb, size_t section1_size_entry, size_t section2_size_entry) {
    sb->cfg.section1_entry_size     = section1_size_entry;
    sb->cfg.section2_entry_size     = section2_size_entry;
    sb->cfg.section3_entry_size     = 0x08;
}
static void config_sb_audio_fs(ubi_sb_header* sb, off_t streamed_flag, off_t software_flag, off_t loop_flag) {
    /* audio header with standard flags */
    sb->cfg.audio_streamed_flag     = streamed_flag;
    sb->cfg.audio_software_flag     = software_flag;
    sb->cfg.audio_loop_flag         = loop_flag;
    sb->cfg.audio_streamed_and      = 1;
    sb->cfg.audio_software_and      = 1;
    sb->cfg.audio_loop_and          = 1;
}
static void config_sb_audio_fb(ubi_sb_header* sb, off_t flag_bits, int streamed_and, int software_and, int loop_and) {
    /* audio header with bit flags */
    sb->cfg.audio_streamed_flag     = flag_bits;
    sb->cfg.audio_software_flag     = flag_bits;
    sb->cfg.audio_loop_flag         = flag_bits;
    sb->cfg.audio_streamed_and      = streamed_and;
    sb->cfg.audio_software_and      = software_and;
    sb->cfg.audio_loop_and          = loop_and;
}
static void config_sb_audio_hs(ubi_sb_header* sb, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t stream_name, off_t stream_type) {
    /* audio header with stream name */
    sb->cfg.audio_channels          = channels;
    sb->cfg.audio_sample_rate       = sample_rate;
    sb->cfg.audio_num_samples       = num_samples;
    sb->cfg.audio_num_samples2      = num_samples2;
    sb->cfg.audio_stream_name       = stream_name;
    sb->cfg.audio_stream_type       = stream_type;
}
static void config_sb_audio_he(ubi_sb_header* sb, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t extra_name, off_t stream_type) {
    /* audio header with extra name */
    sb->cfg.audio_channels          = channels;
    sb->cfg.audio_sample_rate       = sample_rate;
    sb->cfg.audio_num_samples       = num_samples;
    sb->cfg.audio_num_samples2      = num_samples2;
    sb->cfg.audio_extra_name        = extra_name;
    sb->cfg.audio_stream_type       = stream_type;
}
static void config_sb_audio_fb_ps2(ubi_sb_header* sb, off_t flag_bits, int streamed_and, int software_and, int loop_and, int hwmodule_and) {
    /* audio header with bit flags */
    sb->cfg.audio_streamed_flag     = flag_bits;
    sb->cfg.audio_software_flag     = flag_bits;
    sb->cfg.audio_loop_flag         = flag_bits;
    sb->cfg.audio_hwmodule_flag     = flag_bits;
    sb->cfg.audio_streamed_and      = streamed_and;
    sb->cfg.audio_software_and      = software_and;
    sb->cfg.audio_loop_and          = loop_and;
    sb->cfg.audio_hwmodule_and      = hwmodule_and;
}
static void config_sb_audio_ps2_bnm(ubi_sb_header *sb, off_t flag_bits, int streamed_and, int cd_streamed_and, int loop_and, off_t channels, off_t sample_rate) {
    /* bit flags, channels and sample rate */
    sb->cfg.audio_streamed_flag     = flag_bits;
    sb->cfg.audio_cd_streamed_flag  = flag_bits;
    sb->cfg.audio_loop_flag         = flag_bits;
    sb->cfg.audio_streamed_and      = streamed_and;
    sb->cfg.audio_cd_streamed_and   = cd_streamed_and;
    sb->cfg.audio_loop_and          = loop_and;
    sb->cfg.audio_channels          = channels;
    sb->cfg.audio_sample_rate       = sample_rate;
}
static void config_sb_audio_ps2_old(ubi_sb_header *sb, off_t flag_bits, int streamed_and, int loop_and, int loc_and, int stereo_and, off_t pitch, off_t sample_rate) {
    /* bit flags, sample rate only */
    sb->cfg.audio_streamed_flag     = flag_bits;
    sb->cfg.audio_loop_flag         = flag_bits;
    sb->cfg.audio_loc_flag          = flag_bits;
    sb->cfg.audio_stereo_flag       = flag_bits;
    sb->cfg.audio_streamed_and      = streamed_and;
    sb->cfg.audio_loop_and          = loop_and;
    sb->cfg.audio_loc_and           = loc_and;
    sb->cfg.audio_stereo_and        = stereo_and;
    sb->cfg.audio_pitch             = pitch;
    sb->cfg.audio_sample_rate       = sample_rate;
}
static void config_sb_sequence(ubi_sb_header* sb, off_t sequence_count, off_t entry_size) {
    /* sequence header and chain table */
    sb->cfg.sequence_sequence_loop  = sequence_count - 0x10;
    sb->cfg.sequence_sequence_single= sequence_count - 0x0c;
    sb->cfg.sequence_sequence_count = sequence_count;
    sb->cfg.sequence_entry_size     = entry_size;
    sb->cfg.sequence_entry_number   = 0x00;
    if (sb->is_bnm || sb->is_dat || sb->is_ps2_bnm) {
        sb->cfg.sequence_sequence_loop  = sequence_count - 0x0c;
        sb->cfg.sequence_sequence_single= sequence_count - 0x08;
    } else if (sb->is_blk) {
        sb->cfg.sequence_sequence_loop  = sequence_count - 0x14;
        sb->cfg.sequence_sequence_single= sequence_count - 0x0c;
    }
}
static void config_sb_layer_hs(ubi_sb_header* sb, off_t layer_count, off_t stream_size, off_t stream_offset, off_t stream_name) {
    /* layer headers with stream name */
    sb->cfg.layer_layer_count       = layer_count;
    sb->cfg.layer_stream_size       = stream_size;
    sb->cfg.layer_stream_offset     = stream_offset;
    sb->cfg.layer_stream_name       = stream_name;
}
static void config_sb_layer_he(ubi_sb_header* sb, off_t layer_count, off_t stream_size, off_t stream_offset, off_t extra_name) {
    /* layer headers with extra name */
    sb->cfg.layer_layer_count       = layer_count;
    sb->cfg.layer_stream_size       = stream_size;
    sb->cfg.layer_stream_offset     = stream_offset;
    sb->cfg.layer_extra_name        = extra_name;
}
static void config_sb_layer_sh(ubi_sb_header* sb, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples) {
    /* layer sub-headers in extra table */
    sb->cfg.layer_entry_size        = entry_size;
    sb->cfg.layer_sample_rate       = sample_rate;
    sb->cfg.layer_channels          = channels;
    sb->cfg.layer_stream_type       = stream_type;
    sb->cfg.layer_num_samples       = num_samples;
}
static void config_sb_layer_ps2_old(ubi_sb_header *sb, off_t loc_flag, int loc_and, off_t layer_count, off_t pitch) {
    /* no name, no layer headers */
    sb->cfg.layer_loc_flag          = loc_flag;
    sb->cfg.layer_loc_and           = loc_and;
    sb->cfg.layer_layer_count       = layer_count;
    sb->cfg.layer_pitch             = pitch;
}
static void config_sb_silence_i(ubi_sb_header* sb, off_t duration) {
    /* silence headers in int value */
    sb->cfg.silence_duration_int    = duration;
}
static void config_sb_silence_f(ubi_sb_header* sb, off_t duration) {
    /* silence headers in float value */
    sb->cfg.silence_duration_float  = duration;
}
static void config_sb_random_old(ubi_sb_header* sb, off_t sequence_count, off_t entry_size) {
    sb->cfg.random_sequence_count = sequence_count;
    sb->cfg.random_entry_size = entry_size;
    sb->cfg.random_percent_int = 1;
}

static int check_project_file(STREAMFILE *sf_header, const char *name, int has_localized_banks) {
    STREAMFILE *sf_test = open_streamfile_by_filename(sf_header, name);
    if (sf_test) {
        close_streamfile(sf_test);
        return 1;
    }

    if (has_localized_banks) { /* try again for localized subfolders */
        char buf[PATH_LIMIT];
        snprintf(buf, PATH_LIMIT, "../%s", name);
        sf_test = open_streamfile_by_filename(sf_header, buf);
        if (sf_test) {
            close_streamfile(sf_test);
            return 1;
        }
    }

    return 0;
}


/* Each entry in section1/2 has a type of 16b+16b group+sound identifier. May start from 0 (rarely) but
 * always are low-ish numbers, so can be used to a point to detect if entries are correct with some entry_size. */
static int test_version_sb_entry(ubi_sb_header* sb, STREAMFILE* sf, uint32_t offset, int count, uint32_t entry_size) {
    read_u32_t read_u32 = sb->big_endian ? read_u32be : read_u32le;
    uint32_t prev_group = 0;
    int i;

    prev_group = 0;
    for (i = 0; i < count; i++) {
        uint32_t curr = read_u32(offset, sf);
        uint16_t group, sound;

        if (i > 1 && curr == 0)
            return 0;

        /* max seen in ~0x0200 */
        group = (curr >> 16) & 0xFFFF;
        sound = (curr >>  0) & 0xFFFF;
        if (group > 0x1000 || sound > 0x1000)
            return 0;

        /* sounds aren't always ordered, but seems groups are */
        if (prev_group && group < prev_group)
            return 0;

        prev_group = group;
        offset += entry_size;
    }

    return 1;
}

/* Checks if matches entry sizes, for cases where same ID is reused. Only for SB fow now. */
static int test_version_sb(ubi_sb_header* sb, STREAMFILE* sf, uint32_t section1_size_entry, uint32_t section2_size_entry) {
    uint32_t offset;

    if (!init_sb_header(sb, sf))
        return 0;

    if (sb->section2_num == 0) /* no waves = no point to detect */
        return 0;

    offset = sb->section1_offset;
    if (!test_version_sb_entry(sb, sf, offset, sb->section1_num, section1_size_entry))
        return 0;

    offset = sb->section1_offset + sb->section1_num * section1_size_entry;
    if (!test_version_sb_entry(sb, sf, offset, sb->section2_num, section2_size_entry))
        return 0;

    return 1;
}

static int init_sb_header(ubi_sb_header* sb, STREAMFILE* sf) {
    read_u32_t read_u32 = sb->big_endian ? read_u32be : read_u32le;

    if (sb->header_init)
        return 1;

    if (sb->version <= 0x0000000B) {
        sb->section1_num  = read_u32(0x04, sf);
        sb->section2_num  = read_u32(0x0c, sf);
        sb->section3_num  = read_u32(0x14, sf);
        sb->sectionX_size = read_u32(0x1c, sf);

        sb->section1_offset = 0x20;
    }
    else if (sb->version <= 0x000A0000) {
        sb->section1_num  = read_u32(0x04, sf);
        sb->section2_num  = read_u32(0x08, sf);
        sb->section3_num  = read_u32(0x0c, sf);
        sb->sectionX_size = read_u32(0x10, sf);
        sb->flag1         = read_u32(0x14, sf);

        sb->section1_offset = 0x18;
    }
    else {
        sb->section1_num  = read_u32(0x04, sf);
        sb->section2_num  = read_u32(0x08, sf);
        sb->section3_num  = read_u32(0x0c, sf);
        sb->sectionX_size = read_u32(0x10, sf);
        sb->flag1         = read_u32(0x14, sf);
        sb->flag2         = read_u32(0x18, sf);

        sb->section1_offset = 0x1c;
    }

    if (sb->section1_num > SB_MAX_SUBSONGS || sb->section2_num > SB_MAX_SUBSONGS || sb->section3_num > SB_MAX_SUBSONGS)
        return 0;

    sb->header_init = 1;
    return 1;
}


static int config_sb_version(ubi_sb_header* sb, STREAMFILE* sf) {
    int is_ttse_pc = 0;
    int is_bia_ps2 = 0, is_biadd_psp = 0;
    int is_sc2_ps2_gc = 0;
    int is_sc4_pc_online = 0;
    int is_myst4_pc = 0;

    /* Most of the format varies with almost every game + platform (struct serialization?).
     * Support is configured case-by-case as offsets/order/fields only change slightly,
     * and later games may remove fields. We only configure those actually needed. 
     *
     * Various type use "chains" of entry numbers (defined in the extra table).
     * Its format also depends on type.
     */

    /* Header types found in section2 (possibly called "resource headers"):
     * 
     * Type 01 (old/new):
     * Single audio header, external or internal, part of a chain or single. Format:
     * - fixed part (id, type, stream size, extra offset, stream offset)
     * - flags (as bitflags or in separate fields, around ~6 observed flags)
     * - samples+samples (loop+total) + size+size (roughly equal to stream size)
     * - bitrate (total sample rate)
     * - base info (sample rate, pcm bits?, channels, codec)
     * - external filename or internal filename on some platforms (earlier versions)
     * - external filename offset in the extra table (later versions)
     * - end flags?
     * A few games (Splinter Cell, Rainbow Six) have wrong voice sample rates,
     * maybe there is some pitch value too.
     *
     * Type 02 (old?/new):
     * Chain, possibly to play with config (ex: type 08 (float 0.3) + 01)
     *
     * Type 03 (new), 09? (old): 
     * Chain, other way to play things? (ex: type 03 + 04)
     *
     * Type 04 (old?/new), 0a (old):
     * Table of N types + chance % (sums to 65536), to play one as random. Usually N
     * voice/sfx variations like death screams, or sequence alts.
     *
     * Type 05 (new), 0c (old): sequences
     * N audio segment, normally with lead-in but not lead-outs.  Sequences can reuse
     * segments (internal loops), or can be single entries (full song or leads).
     * Sequences seem to include only music or cutscenes, so even single entries can be
     * useful to parse, since the readable name would make them stand out. Format:
     * - extra offset to chain
     * - loop segment
     * - non-looping flag
     * - sequence count
     * - ID-like fields in the header and sequence table may point to other chains?
     *
     * Type 06 (new), 0d (old):
     * Layer header, stream divided into N equal parts in a blocked format. Format:
     * - fixed part (id, type)
     * - extra offset to layer info (sample rate, pcm bits?, channels, codec, samples)
     * - layer count
     * - sometimes total channels, bitrate, etc
     * - flags?
     * - stream size + stream offset
     * - external filename, or filename offset in the extra table
     * Layer blocks are handled separatedly as the format doesn't depend on sb's version/platform.
     * Some values may be flags/config as multiple 0x06 can point to the same layer, with different 'flags'?
     *
     * Type 07 (new), 0e (old):
     * Another chain of something (single entry?), rare.
     *
     * Type 08 (new), 0f (old):
     * Silence, with a value representing duration (no sample rate/channels/extra table/etc given).
     * Typically used in chains to extend play time of another audio.
     * For older games 08 is used for something else (maybe equivalent to 02?)
     */

    /* debug strings reference:
     * - TYPE_SAMPLE: should be 0x01 (also "sound resource")
     * - TYPE_MULTITRACK: should be 0x06/0x0d/0x0b (also "multilayer resource")
     * - TYPE_SILENCE: should be 0x08
     * sequences may be "theme resource"
     * "class descriptor" is referenced too.
     *
     * Type names from .bnm (.sb's predecessor):
     * 0: TYPE_INVALID
     * 1: TYPE_SAMPLE
     * 2: TYPE_MIDI
     * 3: TYPE_CDAUDIO
     * 4: TYPE_SEQUENCE (sfx chain?)
     * 5: TYPE_SWITCH_OLD
     * 6: TYPE_SPLIT
     * 7: TYPE_THEME_OLD
     * 8: TYPE_SWITCH
     * 9: TYPE_THEME_OLD2
     * A: TYPE_RANDOM
     * B: TYPE_THEME0 (sequence)
     * Only 1, 2, 4, 9, A and B are known.
     * 2 is used rarely in Donald Duck's demo and point to a .mdx (midi?)
     * 9 is used in Tonic Trouble Special Edition
     * Others are common.
     */

    /* All types may contain memory garbage, making it harder to identify fields (platforms
     * and games are affected differently by this). Often types contain memory from the previous
     * type header unless overwritten, random memory, or default initialization garbage.
     * So if some non-audio type looks like audio it's probably repeating old data.
     * This even happens for common fields (ex. type 6 at 0x08 has prev garbage, not stream size). */

    /* games <= 0x00100000 seem to use old types, rest new types */

    if (sb->is_bnm || sb->is_dat || sb->is_ps2_bnm) {
        /* these all have names in BNK_%num% format and can reference each other by index */
        sb->has_numbered_banks = 1;
    }

    /* maybe 0x20/0x24 for some but ok enough (null terminated) */
    sb->cfg.resource_name_size          = 0x28; /* min for Brother in Arms 2 (PS2) */

    /* represents map style (1=first, 2=mid, 3=latest) */
    if (sb->version <= 0x00000007)
        sb->cfg.map_version             = 1;
    else if (sb->version < 0x00150000)
        sb->cfg.map_version             = 2;
    else
        sb->cfg.map_version             = 3;

    sb->cfg.map_entry_size = (sb->cfg.map_version < 2) ? 0x30 : 0x34;
    sb->cfg.map_name = 0x10;
    if (sb->is_blk) {
        sb->cfg.map_entry_size = 0x30;
    }

    if (sb->is_bnm || sb->is_blk || sb->is_dat) {
        sb->cfg.audio_internal_flag     = 0x08;
        sb->cfg.audio_stream_size       = 0x0c;
        sb->cfg.audio_stream_offset     = 0x10;
      //sb->cfg.audio_extra_offset      = 0x10;
      //sb->cfg.audio_extra_size        = 0x0c;

        sb->cfg.sequence_extra_offset   = 0x10;
      //sb->cfg.sequence_extra_size     = 0x0c;

        //sb->cfg.layer_extra_offset    = 0x10;
        //sb->cfg.layer_extra_size      = 0x0c;

        sb->cfg.random_extra_offset     = 0x10;
      //sb->cfg.random_extra_size       = 0x0c;
    } else if (sb->is_ps2_bnm) {
        sb->cfg.audio_stream_size       = 0x2c;
        sb->cfg.audio_stream_offset     = 0x30;

        sb->cfg.sequence_extra_offset   = 0x10;
      //sb->cfg.sequence_extra_size     = 0x0c;

        sb->cfg.random_extra_offset     = 0x10;
      //sb->cfg.random_extra_size       = 0x0c;
    } else if (sb->version <= 0x00000007) {
        sb->cfg.audio_internal_flag     = 0x08;
        sb->cfg.audio_stream_size       = 0x0c;
        sb->cfg.audio_extra_offset      = 0x10;
        sb->cfg.audio_stream_offset     = 0x14;

        sb->cfg.sequence_extra_offset   = 0x10;

        sb->cfg.layer_extra_offset      = 0x10;
    } else {
        sb->cfg.audio_stream_size       = 0x08;
        sb->cfg.audio_extra_offset      = 0x0c;
        sb->cfg.audio_stream_offset     = 0x10;

        sb->cfg.sequence_extra_offset   = 0x0c;

        sb->cfg.layer_extra_offset      = 0x0c;
    }

    sb->allowed_types[0x01] = 1;
    sb->allowed_types[0x05] = 1;
    sb->allowed_types[0x0c] = 1;
    sb->allowed_types[0x06] = 1;
    sb->allowed_types[0x0d] = 1;
  //sb->allowed_types[0x08] = 1; /* only needed inside sequences */
  //sb->allowed_types[0x0f] = 1;
    if (sb->is_bnm || sb->is_dat || sb->is_ps2_bnm) {
      //sb->allowed_types[0x0a] = 1; /* only needed inside sequences */
        sb->allowed_types[0x0b] = 1;
    }

#if 0
    {
        STREAMFILE* test_sf;
        test_sf= open_streamfile_by_filename(sf, ".no_audio.sbx");
        if (test_sf) { sb->allowed_types[0x01] = 0; close_streamfile(test_sf); }

        test_sf= open_streamfile_by_filename(sf, ".no_sequence.sbx");
        if (test_sf) { sb->allowed_types[0x05] = sb->allowed_types[0x0c] = 0; close_streamfile(test_sf); }

        test_sf= open_streamfile_by_filename(sf, ".no_layer.sbx");
        if (test_sf) { sb->allowed_types[0x06] = sb->allowed_types[0x0d] = 0; close_streamfile(test_sf); }
    }
#endif

    /* two configs with same id; use SND file as identifier */
    if (sb->version == 0x00000000 && sb->platform == UBI_PC) {
        if (check_project_file(sf, "Dino.lcb", 0)) {
            sb->version = 0x00000200; /* some files in Dinosaur use this, probably final version */
        }
    }

    /* memory garbage found in F1 Racing Simulation */
    if ((sb->version == 0xAAAAAAAA && sb->platform == UBI_PC) ||
        (sb->version == 0xCDCDCDCD && sb->platform == UBI_PC)) {
        sb->version = 0x00000000;
    }

    /* Tonic Touble beta has garbage instead of version */
    if (sb->is_bnm && sb->version > 0x00000000 && sb->platform == UBI_PC) {
        if (check_project_file(sf, "ED_MAIN.LCB", 0)) {
            is_ttse_pc = 1;
            sb->version = 0x00000000;
        }
    }


    /* Tonic Trouble Special Edition (1998)(PC)-bnm */
    if (sb->version == 0x00000000 && sb->platform == UBI_PC && is_ttse_pc) {
        config_sb_entry(sb, 0x20, 0x5c);

        config_sb_audio_fs(sb, 0x2c, 0x00, 0x30);
        config_sb_audio_hs(sb, 0x42, 0x3c, 0x38, 0x38, 0x48, 0x44);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x24, 0x18);

        //config_sb_random_old(sb, 0x18, 0x0c);

        /* no layers */
        //todo type 9 needed
        //todo MPX don't set stream size?
        return 1;
    }

    /* F1 Racing Simulation (1997)(PC)-bnm [not TTSE version] */
    /* Rayman 2: The Great Escape (1999)(PC)-bnm */
    /* Tonic Trouble (1999)(PC)-bnm */
    /* Donald Duck: Goin' Quackers (2000)(PC)-bnm */
    /* Disney's Dinosaur (2000)(PC)-bnm */
    if ((sb->version == 0x00000000 && sb->platform == UBI_PC) ||
        (sb->version == 0x00000200 && sb->platform == UBI_PC)) {
        config_sb_entry(sb, 0x20, 0x5c);
        if (sb->version == 0x00000200)
            config_sb_entry(sb, 0x20, 0x60);

        config_sb_audio_fs(sb, 0x2c, 0x00, 0x30);
        config_sb_audio_hs(sb, 0x42, 0x3c, 0x34, 0x34, 0x48, 0x44);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x24, 0x18);

        config_sb_random_old(sb, 0x18, 0x0c); /* Rayman 2 needs it for rare sequence ends (ex. Bnk_31.bnm) */

        /* no layers */
        return 1;
    }

    /* The Jungle Book: Rhythm N'Groove (2000)(PC)-bnm */
    if (sb->version == 0x00060409 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x24, 0x64);

        config_sb_audio_fs(sb, 0x2c, 0x00, 0x30);
        config_sb_audio_hs(sb, 0x4E, 0x48, 0x34, 0x34, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x2c, 0x1c);

        /* no layers */
        return 1;
    }

    /* not again... */
    if (sb->version == 0x00000000 && sb->platform == UBI_DC) {
        /* check if there's a matching KAT, crap but works */
        STREAMFILE *test_sf = open_streamfile_by_ext(sf, "kat");
        if (test_sf) {
            sb->version = 0x00000200; /* assumed */
            close_streamfile(test_sf);
        }
    }

    /* Rayman 2: The Great Escape (2000)(DC)-dat */
    /* Donald Duck: Goin' Quackers (2000)(DC)-dat */
    /* Disney's Dinosaur (2000)(DC)-dat */
    if ((sb->version == 0x00000000 && sb->platform == UBI_DC) ||
        (sb->version == 0x00000200 && sb->platform == UBI_DC)) {
        config_sb_entry(sb, 0x20, 0x64);
        if (sb->version == 0x00000200)
            config_sb_entry(sb, 0x20, 0x68);

        config_sb_audio_fs(sb, 0x2c, 0x00, 0x30);
        config_sb_audio_hs(sb, 0x42, 0x3c, 0x34, 0x34, 0x48, 0x44);
        /* has internal names but they're partially overwritten by subbank index */

        config_sb_sequence(sb, 0x24, 0x18);

        config_sb_random_old(sb, 0x18, 0x0c);

        /* no layers */
        return 1;
    }

    /* Rayman 2: Revolution (2000)(PS2)-bnm */
    /* Disney's Dinosaur (2000)(PS2)-bnm */
    /* Hype: The Time Quest (2001)(PS2)-bnm */
    if (sb->version == 0x32787370 && sb->platform == UBI_PS2) {
        sb->version = 0x00000000; /* for convenience */
        config_sb_entry(sb, 0x1c, 0x44);

        config_sb_audio_ps2_bnm(sb, 0x18, (1 << 5), (1 << 6), (1 << 7), 0x20, 0x22);
        sb->cfg.audio_interleave = 0x400;

        config_sb_sequence(sb, 0x24, 0x14);

        /* no layers */
        return 1;
    }

    /* Batman: Vengeance (2001)(PC)-map */
    /* Batman: Vengeance (2001)(Xbox)-map */
    if ((sb->version == 0x00000003 && sb->platform == UBI_PC) ||
        (sb->version == 0x00000003 && sb->platform == UBI_XBOX)) {
        config_sb_entry(sb, 0x40, 0x68);

        config_sb_audio_fs(sb, 0x30, 0x00, 0x34);
        config_sb_audio_hs(sb, 0x52, 0x4c, 0x38, 0x40, 0x58, 0x54);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x2c, 0x1c);

        config_sb_layer_hs(sb, 0x20, 0x4c, 0x44, 0x34);
        config_sb_layer_sh(sb, 0x1c, 0x04, 0x0a, 0x0c, 0x18);
        return 1;
    }

    /* Donald Duck: Goin' Quackers (2000)(PS2)-blk */
    /* The Jungle Book: Rhythm N'Groove (2001)(PS2)-blk */
    if (sb->version == 0x00000003 && sb->platform == UBI_PS2 && sb->is_blk) {
        config_sb_entry(sb, 0x20, 0x40);

        config_sb_audio_ps2_old(sb, 0x18, (1 << 4), (1 << 5), (1 << 6), (1 << 7), 0x1c, 0x20);
        sb->cfg.audio_interleave = 0x800;
        sb->is_ps2_old = 1; /* yikes */

        config_sb_sequence(sb, 0x2c, 0x18); /* this is normal enough */

        config_sb_layer_ps2_old(sb, 0x18, (1 << 0), 0x1c, 0x20);
        return 1;
    }

    /* Batman: Vengeance (2001)(PS2)-map */
    /* Disney's Tarzan: Untamed (2001)(PS2)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x30, 0x3c);

        config_sb_audio_ps2_old(sb, 0x1c, (1 << 4), (1 << 5), (1 << 6), (1 << 7), 0x20, 0x24);
        sb->cfg.audio_interleave = 0x800;
        sb->is_ps2_old = 1;

        config_sb_sequence(sb, 0x2c, 0x18);

        config_sb_layer_ps2_old(sb, 0x1c, (1 << 0), 0x20, 0x24);
        return 1;
    }

    /* Disney's Tarzan: Untamed (2001)(GC)-map */
    /* Batman: Vengeance (2001)(GC)-map */
    /* Donald Duck: Goin' Quackers (2002)(GC)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x40, 0x6c);

        config_sb_audio_fs(sb, 0x30, 0x00, 0x34);
        config_sb_audio_hs(sb, 0x56, 0x50, 0x48, 0x48, 0x5c, 0x58); /* 0x38 may be num samples too? */

        config_sb_sequence(sb, 0x2c, 0x1c);

        config_sb_layer_hs(sb, 0x20, 0x4c, 0x44, 0x34);
        config_sb_layer_sh(sb, 0x1c, 0x04, 0x0a, 0x0c, 0x18);
        return 1;
    }

    /* Myst III: Exile (2001)(PS2)-map */
    if (sb->version == 0x00000004 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x34, 0x70);

        config_sb_audio_fb(sb, 0x1c, (1 << 4), 0, (1 << 5));
        config_sb_audio_hs(sb, 0x24, 0x28, 0x34, 0x3c, 0x44, 0x6c);

        config_sb_sequence(sb, 0x2c, 0x24);
        return 1;
    }

    /* Splinter Cell (2002)(PC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(PC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x58, 0x80);

        config_sb_audio_fs(sb, 0x28, 0x00, 0x2c);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x2c, 0x34);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);
        return 1;
    }

    /* Splinter Cell (2002)(Xbox)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(Xbox)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x58, 0x78);

        config_sb_audio_fs(sb, 0x28, 0x00, 0x2c);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x2c, 0x34);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);
        return 1;
    }

    /* SC:PT PS2/GC has some quirks, noooo (lame autodetection but this stuff is too hard) */
    if ((sb->version == 0x00000007 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00000007 && sb->platform == UBI_GC) ) {
        int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

        /* both SC:PT's LMx and SMx have 33 maps, SC1 doesn't */
        is_sc2_ps2_gc = read_32bit(0x08, sf) == 0x21;

        /* could also load ECHELON.SP1/Echelon.SP3 and test BE 0x04 == 0x00ACBF77,
         * but it's worse for localization subdirs without it */
    }

    /* Splinter Cell (2002)(PS2)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(PS2)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x40, 0x70);

        config_sb_audio_fb(sb, 0x1c, (1 << 2), 0, (1 << 3));
        config_sb_audio_hs(sb, 0x24, 0x28, 0x34, 0x3c, 0x44, 0x6c); /* num_samples may be null */

        config_sb_sequence(sb, 0x2c, 0x30);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);

        if (is_sc2_ps2_gc) {
            sb->cfg.map_entry_size = 0x38;
            sb->cfg.map_name = 0x18;
            sb->cfg.has_rs_files = 1;
            sb->cfg.audio_ram_streamed_flag = 0x1c;
            sb->cfg.audio_ram_streamed_and = (1 << 3);
            sb->cfg.audio_loop_and = (1 << 4);
            /* some RAM sounds have bad sizes (ex #252, #10874) */
            sb->cfg.layer_hijack = LAYER_HIJACK_SCPT_PS2; /* some amb .ss1 layers (ex. #226, not #1927) have mixed garbage */
        }
        return 1;
    }

    /* Splinter Cell (2002)(GC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(GC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x58, 0x78);

        config_sb_audio_fs(sb, 0x24, 0x00, 0x28);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x2c, 0x34, 0x50, 0x4c);

        config_sb_sequence(sb, 0x2c, 0x34);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);

        if (is_sc2_ps2_gc) {
            sb->cfg.map_entry_size = 0x38;
            sb->cfg.map_name = 0x18;
            sb->cfg.audio_streamed_and = 0x01000000; /* did somebody forget about BE? */
        }
        return 1;
    }

    /* Tom Clancy's Rainbow Six 3: Raven Shield + addons (2003)(PC)-bank */
    if (sb->version == 0x0000000B && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x5c, 0x7c);

        config_sb_audio_fs(sb, 0x24, 0x00, 0x28);
        config_sb_audio_hs(sb, 0x46, 0x40, 0x2c, 0x34, 0x4c, 0x48);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x34);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* Prince of Persia: The Sands of Time (Demo)(2003)(Xbox)-bank */
    if (sb->version == 0x0000000D && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x5c, 0x74);

        config_sb_audio_fs(sb, 0x24, 0x00, 0x28);
        config_sb_audio_hs(sb, 0x46, 0x40, 0x2c, 0x34, 0x4c, 0x48);
        sb->cfg.audio_has_internal_names = 1;

        //config_sb_sequence(sb, 0x28, 0x34);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* Prince of Persia: The Sands of Time (Demo)(2003)(Xbox)-bank */
    if (sb->version == 0x000A0000 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x78);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x2c);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);

        //config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* Prince of Persia: Sands of Time (2003)(PC)-bank 0x000A0004 / 0x000A0002 (just in case) */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PC) ||
        (sb->version == 0x000A0004 && sb->platform == UBI_PC)) {
        config_sb_entry(sb, 0x64, 0x80);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* two configs with same id; use project file as identifier */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2) {
        if (check_project_file(sf, "BIAAUDIO.SP1", 1)) {
            is_bia_ps2 = 1;
        }
    }

    /* Prince of Persia: The Sands of Time (2003)(PS2)-bank 0x000A0004 / 0x000A0002 (POP1 port/Demo) */
    /* Tom Clancy's Rainbow Six 3 (2003)(PS2)-bank 0x000A0007 */
    /* Tom Clancy's Ghost Recon 2 (2004)(PS2)-bank 0x000A0007 */
    /* Splinter Cell: Pandora Tomorrow-online (2004)(PS2)-bank 0x000A0008 */
    /* Prince of Persia: Warrior Within (Demo)(2004)(PS2)-bank 0x00100000 */
    /* Prince of Persia: Warrior Within (2004)(PS2)-bank 0x00120009 */
    /* Horsez / Champion Dreams: First to Ride / Pipa Funnell: Take the Reins (2006)(PS2)-bank 0x0012000c */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PS2) ||
        (sb->version == 0x000A0004 && sb->platform == UBI_PS2) ||
        (sb->version == 0x000A0007 && sb->platform == UBI_PS2 && !is_bia_ps2) ||
        (sb->version == 0x000A0008 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00100000 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00120009 && sb->platform == UBI_PS2) ||
        (sb->version == 0x0012000c && sb->platform == UBI_PS2)) {
        config_sb_entry(sb, 0x48, 0x6c);

        config_sb_audio_fb_ps2(sb, 0x18, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_hs(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x68); /* num_samples may be null */

        config_sb_sequence(sb, 0x28, 0x10);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        config_sb_silence_i(sb, 0x18);
        return 1;
    }

    /* Brothers in Arms: Road to Hill 30 (2005)(PS2)-bank */
    /* Brothers in Arms: Earned in Blood (2005)(PS2)-bank */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2 && is_bia_ps2) {
        config_sb_entry(sb, 0x5c, 0x14c);

        config_sb_audio_fb_ps2(sb, 0x18, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_hs(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x148); /* num_samples may be null */

        config_sb_sequence(sb, 0x28, 0x10);

        config_sb_layer_hs(sb, 0x20, 0x140, 0x138, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        sb->cfg.is_padded_section1_offset = 1;
        sb->cfg.is_padded_section2_offset = 1;
        sb->cfg.is_padded_section3_offset = 1;
        sb->cfg.is_padded_sectionX_offset = 1;
        sb->cfg.is_padded_sounds_offset = 1;
        return 1;
    }

    /* Batman: Rise of Sin Tzu (2003)(Xbox)-map */
    if (sb->version == 0x000A0003 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x80);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x34);
        config_sb_audio_hs(sb, 0x52, 0x4c, 0x38, 0x40, 0x58, 0x54);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        //todo some sequences mix 1ch and 2ch (voices?)
        return 1;
    }

    /* Prince of Persia: The Sands of Time (2003)(Xbox)-bank 0x000A0004 / 0x000A0002 (POP1 port/Demo) */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_XBOX) ||
        (sb->version == 0x000A0004 && sb->platform == UBI_XBOX)) {
        config_sb_entry(sb, 0x64, 0x78);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x2c);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* Batman: Rise of Sin Tzu (2003)(GC)-map 0x000A0002 */
    /* Prince of Persia: The Sands of Time (2003)(GC)-bank 0x000A0004 / 0x000A0002 (POP1 port) */
    /* Tom Clancy's Rainbow Six 3 (2003)(GC)-bank 0x000A0007 */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_GC) ||
        (sb->version == 0x000A0004 && sb->platform == UBI_GC) ||
        (sb->version == 0x000A0007 && sb->platform == UBI_GC)) {
        config_sb_entry(sb, 0x64, 0x74);

        config_sb_audio_fs(sb, 0x20, 0x24, 0x28);
        config_sb_audio_hs(sb, 0x46, 0x40, 0x2c, 0x34, 0x4c, 0x48);

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        config_sb_silence_i(sb, 0x18);
        return 1;
    }

    /* Tom Clancy's Rainbow Six 3 (2003)(Xbox)-bank */
    if (sb->version == 0x000A0007 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x8c);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x40);
        config_sb_audio_hs(sb, 0x5e, 0x58, 0x44, 0x4c, 0x64, 0x60);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        config_sb_silence_i(sb, 0x18);
        return 1;
    }

    /* Myst IV: Revelation (Demo)(2004)(PC)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0xa4);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within (Demo)(2004)(Xbox)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x68, 0x90);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x40);
        config_sb_audio_hs(sb, 0x60, 0x58, 0x44, 0x4c, 0x68, 0x64);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);
        return 1;
    }

    /* two configs with same id; try to autodetect or use project file as identifier */
    if (sb->version == 0x00120006 && sb->platform == UBI_PC) {
        if (test_version_sb(sb, sf, 0x6c, 0xa4) || check_project_file(sf, "gamesnd_myst4.sp0", 1)) {
            is_myst4_pc = 1;
        }
    }

    /* Myst IV: Revelation (2004)(PC)-bank 0x00120006 */
    /* Prince of Persia: Warrior Within (Demo)(2004)(PC)-bank 0x00120006 */
    /* Prince of Persia: Warrior Within (2004)(PC)-bank 0x00120009 */
    if ((sb->version == 0x00120006 && sb->platform == UBI_PC) ||
        (sb->version == 0x00120009 && sb->platform == UBI_PC)) {
        config_sb_entry(sb, 0x6c, 0x84);
        if (is_myst4_pc)
            config_sb_entry(sb, 0x6c, 0xa4);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(Xbox)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x6c, 0x90);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x40);
        config_sb_audio_hs(sb, 0x60, 0x58, 0x44, 0x4c, 0x68, 0x64);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);
        return 1;
    }

    /* Prince of Persia: Warrior Within (2004)(GC)-bank */
    if (sb->version == 0x00120009 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x6c, 0x78);

        config_sb_audio_fs(sb, 0x20, 0x24, 0x28);
        config_sb_audio_hs(sb, 0x48, 0x40, 0x2c, 0x34, 0x50, 0x4c);

        config_sb_sequence(sb, 0x28, 0x14);
        return 1;
    }

    /* two configs with same id and both sb4/sm4; use project file as identifier */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP) {
        if (check_project_file(sf, "BIAAUDIO.SP4", 1)) {
            is_biadd_psp = 1;
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP)-bank */
    /* Splinter Cell: Essentials (2006)(PSP)-map */
    /* Beowulf: The Game (2007)(PSP)-map */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && !is_biadd_psp) {
        config_sb_entry(sb, 0x68, 0x84);
        if (is_biadd_psp)
            config_sb_entry(sb, 0x80, 0x94);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x1c, 0x60, 0x64, 0x30);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x08, 0x0c, 0x14);
        //todo some .sbX in BiA:DD have bad external stream offsets, but not all (ex. offset 0xE3641 but should be 0x0A26)
        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(PC)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0x60);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_he(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);

        config_sb_sequence(sb, 0x28, 0x14);
        return 1;
    }

    /* Myst IV: Revelation (2005)(Xbox)-bank */
    /* Splinter Cell: Chaos Theory (2005)(Xbox)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x48, 0x4c);

        config_sb_audio_fb(sb, 0x18, (1 << 3), (1 << 4), (1 << 10));
        config_sb_audio_he(sb, 0x38, 0x30, 0x1c, 0x24, 0x40, 0x3c);

        config_sb_sequence(sb, 0x28, 0x10);
        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(PS2)-map */
    if (sb->version == 0x00130001 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x48, 0x4c);

        config_sb_audio_fb_ps2(sb, 0x18, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_he(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x44);

        config_sb_sequence(sb, 0x28, 0x10);

        //config_sb_layer_he(sb, 0x1c, 0x28, 0x30, 0x34);
        //config_sb_layer_sh(sb, 0x18, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Splinter Cell: Chaos Theory (2005)(GC)-map */
    if (sb->version == 0x00130001 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x68, 0x54);

        config_sb_audio_fs(sb, 0x20, 0x24, 0x28);
        config_sb_audio_he(sb, 0x48, 0x40, 0x2c, 0x34, 0x50, 0x4c);

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_he(sb, 0x1c, 0x34, 0x3c, 0x40);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Splinter Cell 3D (2011)(3DS)-map */
    if (sb->version == 0x00130001 && sb->platform == UBI_3DS) {
        config_sb_entry(sb, 0x48, 0x4c);

        config_sb_audio_fb(sb, 0x18, (1 << 2), (1 << 3), (1 << 4));
        config_sb_audio_he(sb, 0x38, 0x30, 0x1c, 0x24, 0x40, 0x3c);

        config_sb_sequence(sb, 0x28, 0x10);

        config_sb_layer_he(sb, 0x1c, 0x28, 0x30, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Tom Clancy's Ghost Recon Advanced Warfighter (2006)(PS2)-bank */
    if (sb->version == 0x00130004 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x48, 0x50);

        config_sb_audio_fb_ps2(sb, 0x18, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_he(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x4c);
        sb->cfg.audio_interleave = 0x8000;

        sb->cfg.is_padded_section1_offset = 1;
        sb->cfg.is_padded_sounds_offset = 1;
        return 1;
    }

    /* Tom Clancy's Ghost Recon Advanced Warfighter (2006)(Xbox)-bank */
    if (sb->version == 0x00130004 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x48, 0x50);

        config_sb_audio_fb(sb, 0x1c, (1 << 3), (1 << 4), (1 << 10));
        config_sb_audio_he(sb, 0x3c, 0x34, 0x20, 0x28, 0x44, 0x40);

        /* what */
        sb->cfg.audio_extra_offset      = 0x10;
        sb->cfg.audio_stream_offset     = 0x14;
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PC)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x34, 0x30);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);

        config_sb_sequence(sb, 0x2c, 0x14);
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(PS2)-bank */
    if (sb->version == 0x00150000 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x48, 0x5c);

        config_sb_audio_fb_ps2(sb, 0x20, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_he(sb, 0x2c, 0x30, 0x3c, 0x44, 0x4c, 0x50);

        config_sb_sequence(sb, 0x2c, 0x10);
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(Xbox)-bank 0x00150000 */
    /* Far Cry Instincts (2005)(Xbox)-bank 0x00150000 */
    /* Splinter Cell: Double Agent (2006)(Xbox)-map 0x00160002 */
    /* Far Cry Instincts: Evolution (2006)(Xbox)-bank 0x00170000 */
    if ((sb->version == 0x00150000 && sb->platform == UBI_XBOX) ||
        (sb->version == 0x00160002 && sb->platform == UBI_XBOX) ||
        (sb->version == 0x00170000 && sb->platform == UBI_XBOX)) {
        config_sb_entry(sb, 0x48, 0x58);

        config_sb_audio_fb(sb, 0x20, (1 << 3), (1 << 4), (1 << 10));
        config_sb_audio_he(sb, 0x44, 0x3c, 0x28, 0x30, 0x4c, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x34, 0x3c);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Prince of Persia: The Two Thrones (2005)(GC)-bank 0x00150000 */
    /* Splinter Cell: Double Agent (2006)(GC)-map 0x00160002 */
    if ((sb->version == 0x00150000 && sb->platform == UBI_GC) || 
        (sb->version == 0x00160002 && sb->platform == UBI_GC)) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x58, 0x50, 0x3c, 0x44, 0x60, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x40, 0x48);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(PS2)-map 0x00160002 */
    /* Open Season (2006)(PS2)-map 0x00180003 */
    /* Open Season (2006)(PSP)-map 0x00180003 */
    /* Shaun White Snowboarding (2008)(PS2)-map 0x00180003 */
    /* Prince of Persia: Rival Swords (2007)(PSP)-bank 0x00180005 */
    /* Rainbow Six Vegas (2007)(PSP)-bank 0x00180006 */
    /* Star Wars: Lethal Alliance (2006)(PSP)-map 0x00180007 */
    if ((sb->version == 0x00160002 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00180003 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00180003 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180005 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180006 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00180007 && sb->platform == UBI_PSP)) {
        config_sb_entry(sb, 0x48, 0x54);

        config_sb_audio_fb_ps2(sb, 0x20, (1 << 2), (1 << 3), (1 << 4), (1 << 5));
        config_sb_audio_he(sb, 0x28, 0x2c, 0x34, 0x3c, 0x44, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);

        config_sb_silence_f(sb, 0x1c);

        /* Rainbow Six Vegas (PSP) has 2 layers with different sample rates, but 2nd layer is silent and can be ignored */
        if (sb->version == 0x00180006 && sb->platform == UBI_PSP)
            sb->cfg.ignore_layer_error = 1;
        return 1;
    }

    /* Tom Clancy's Ghost Recon Advanced Warfighter (2006)(X360)-bank */
    if (sb->version == 0x00170001 && sb->platform == UBI_X360) {
        config_sb_entry(sb, 0x68, 0x70);

        config_sb_audio_fs(sb, 0x2c, 0x30, 0x34);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);
        sb->cfg.audio_xma_offset = 0; /* header is in the extra table */

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x40, 0x48);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x08, 0x0c, 0x14);
        sb->cfg.layer_hijack = LAYER_HIJACK_GRAW_X360; /* WTF!!! layer format different from other layers using same id!!! */
        return 1;
    }

    /* Open Season (2006)(PC)-map */
    if (sb->version == 0x00180003 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x34, 0x30);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* Open Season (2006)(Xbox)-map */
    if (sb->version == 0x00180003 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x48, 0x58);

        config_sb_audio_fb(sb, 0x20, (1 << 3), (1 << 4), (1 << 10));
        config_sb_audio_he(sb, 0x44, 0x3c, 0x28, 0x30, 0x4c, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* Open Season (2006)(GC)-map */
    if (sb->version == 0x00180003 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x58, 0x50, 0x3c, 0x44, 0x60, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* Open Season (2006)(X360)-map */
    if (sb->version == 0x00180003 && sb->platform == UBI_X360) {
        config_sb_entry(sb, 0x68, 0x74);

        config_sb_audio_fs(sb, 0x2c, 0x30, 0x34);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);
        sb->cfg.audio_xma_offset = 0x70;

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* two configs with same id; use project file as identifier */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        if (check_project_file(sf, "Sc4_online_SoundProject.SP0", 1)) {
            is_sc4_pc_online = 1;
        }
    }

    /* Splinter Cell: Double Agent (2006)(PC)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0x7c);
        if (is_sc4_pc_online)
            config_sb_entry(sb, 0x68, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x34, 0x30);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(X360)-map */
    if (sb->version == 0x00180006 && sb->platform == UBI_X360) {
        config_sb_entry(sb, 0x68, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x30, 0x34);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);
        sb->cfg.audio_xma_offset = 0x70;

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Red Steel (2006)(Wii)-bank 0x00180006 */
    /* Splinter Cell: Double Agent (2006)(Wii)-map 0x00180007 */
    /* Open Season (2006)(Wii)-map 0x00180008 */
    if ((sb->version == 0x00180006 && sb->platform == UBI_WII) ||
        (sb->version == 0x00180007 && sb->platform == UBI_WII) ||
        (sb->version == 0x00180008 && sb->platform == UBI_WII)) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x58, 0x50, 0x3c, 0x44, 0x60, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Tom Clancy's Ghost Recon Advanced Warfighter 2 (2007)(X360)-bank */
    if (sb->version == 0x0018000b && sb->platform == UBI_X360) {
        config_sb_entry(sb, 0x68, 0x70);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x3c, 0x40, 0x48, 0x50, 0x58, 0x5c);
        sb->cfg.audio_xma_offset = 0x68;

        config_sb_sequence(sb, 0x2c, 0x14);
        return 1;
    }

    /* TMNT (2007)(PSP)-map 0x00190001 */
    /* Surf's Up (2007)(PSP)-map 0x00190005 */
    if ((sb->version == 0x00190001 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00190005 && sb->platform == UBI_PSP)) {
        config_sb_entry(sb, 0x48, 0x58);

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 4)); /* assumed software_flag */
        config_sb_audio_he(sb, 0x28, 0x2c, 0x34, 0x3c, 0x44, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);
        return 1;
    }

    /* TMNT (2007)(PC)-bank 0x00190002 */
    /* Surf's Up (2007)(PC)-bank 0x00190005 */
    if ((sb->version == 0x00190002 && sb->platform == UBI_PC) ||
        (sb->version == 0x00190005 && sb->platform == UBI_PC)) {
        config_sb_entry(sb, 0x68, 0x74);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x3c, 0x40, 0x48, 0x50, 0x58, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x34, 0x38, 0x40);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* TMNT (2007)(PS2)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x48, 0x5c);

        config_sb_audio_fb_ps2(sb, 0x20, (1 << 2), (1 << 3), (1 << 4), (1 << 5)); /* assumed software_flag */
        config_sb_audio_he(sb, 0x28, 0x2c, 0x34, 0x3c, 0x44, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* TMNT (2007)(GC)-bank */
    /* Surf's Up (2007)(GC)-bank 0x00190005 */
    if ((sb->version == 0x00190002 && sb->platform == UBI_GC) ||
        (sb->version == 0x00190005 && sb->platform == UBI_GC)) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30); /* assumed groud_id */
        config_sb_audio_he(sb, 0x3c, 0x40, 0x48, 0x50, 0x58, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x34, 0x38, 0x40);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* TMNT (2007)(X360)-bank 0x00190002 */
    /* My Word Coach (2007)(Wii)-bank 0x00190002 */
    /* Prince of Persia: Rival Swords (2007)(Wii)-bank 0x00190003 */
    /* Rainbow Six Vegas (2007)(PS3)-bank 0x00190005 */
    /* Surf's Up (2007)(Wii)-bank 0x00190005 */
    /* Surf's Up (2007)(PS3)-bank 0x00190005 */
    /* Surf's Up (2007)(X360)-bank 0x00190005 */
    /* Splinter Cell: Double Agent (2007)(PS3)-map 0x00190005 */
    if ((sb->version == 0x00190002 && sb->platform == UBI_X360) ||
        (sb->version == 0x00190002 && sb->platform == UBI_WII) ||
        (sb->version == 0x00190003 && sb->platform == UBI_WII) ||
        (sb->version == 0x00190005 && sb->platform == UBI_WII) ||
        (sb->version == 0x00190005 && sb->platform == UBI_PS3) ||
        (sb->version == 0x00190005 && sb->platform == UBI_X360)) {
        config_sb_entry(sb, 0x68, 0x70);
        sb->cfg.audio_fix_psx_samples = 1; /* ex. RSV PS3: 3n#10, SC DA PS3 */

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x3c, 0x40, 0x48, 0x50, 0x58, 0x5c);
        sb->cfg.audio_xma_offset = 0x6c;
        sb->cfg.audio_interleave = 0x10;

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x34, 0x38, 0x40);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);
        return 1;
    }

    /* Tom Clancy's Ghost Recon Advanced Warfighter 2 (2007)(PS3)-bank */
    if (sb->version == 0x001A0003 && sb->platform == UBI_PS3) {
        config_sb_entry(sb, 0x6c, 0x78);

        config_sb_audio_fs(sb, 0x30, 0x34, 0x38);
        config_sb_audio_he(sb, 0x40, 0x44, 0x4c, 0x54, 0x5c, 0x60);

        config_sb_sequence(sb, 0x2c, 0x14);

        return 1;
    }

    /* Cranium Kabookii (2007)(Wii)-bank */
    if (sb->version == 0x001A0003 && sb->platform == UBI_WII) {
        config_sb_entry(sb, 0x6c, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x30, 0x34);
        config_sb_audio_he(sb, 0x40, 0x44, 0x4c, 0x54, 0x5c, 0x60);
        return 1;
    }

    /* Naruto: Rise of a Ninja (2007)(X360)-bank */
    /* Rainbow Six Vegas 2 (2008)(PS3)-bank */
    /* Rainbow Six Vegas 2 (2008)(X360)-bank */
    if ((sb->version == 0x001B0001 && sb->platform == UBI_X360) ||
        (sb->version == 0x001C0000 && sb->platform == UBI_PS3) ||
        (sb->version == 0x001C0000 && sb->platform == UBI_X360)) {
        config_sb_entry(sb, 0x64, 0x7c);

        config_sb_audio_fs(sb, 0x28, 0x30, 0x34);
        config_sb_audio_he(sb, 0x44, 0x48, 0x50, 0x58, 0x60, 0x64);
        sb->cfg.audio_xma_offset = 0x78;
        sb->cfg.audio_interleave = 0x10;
        sb->cfg.audio_fix_psx_samples = 1;

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x44, 0x48, 0x54);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);
        return 1;
    }

    /* Michael Jackson: The Experience (2010)(PSP)-map */
    if (sb->version == 0x001D0000 && sb->platform == UBI_PSP) {
        config_sb_entry(sb, 0x40, 0x60);

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 5)); /* assumed software_flag */
        config_sb_audio_he(sb, 0x28, 0x30, 0x38, 0x40, 0x48, 0x4c);
        return 1;
    }

    /* Petz Sports: Dog Playground (2008)(Wii)-bank */
    /* Cloudy with a Chance of Meatballs (2009)(Wii)-bank */
    /* Grey's Anatomy: The Video Game (2009)(Wii)-bank */
    /* NCIS: Based on the TV Series (2011)(Wii)-bank */
    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map */
    if ((sb->version == 0x001D0000 && sb->platform == UBI_PS3) ||
        (sb->version == 0x001D0000 && sb->platform == UBI_WII)) {
        config_sb_entry(sb, 0x5c, 0x80);
        sb->cfg.audio_interleave = 0x10;
        sb->cfg.audio_fix_psx_samples = 1;

        config_sb_audio_fs(sb, 0x28, 0x30, 0x34);
        config_sb_audio_he(sb, 0x44, 0x4c, 0x54, 0x5c, 0x64, 0x68);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x44, 0x48, 0x54);
        config_sb_layer_sh(sb, 0x38, 0x00, 0x04, 0x08, 0x10);
        return 1;
    }

    vgm_logi("UBI SB: unknown SB/SM version+platform %08x (report)\n", sb->version);
    return 0;
}
