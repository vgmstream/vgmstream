#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ubi_sb_streamfile.h"


#define SB_MAX_LAYER_COUNT 16  /* arbitrary max */
#define SB_MAX_CHAIN_COUNT 256 /* +150 exist in Tonic Trouble */

typedef enum { UBI_IMA, UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_DSP, RAW_XBOX, FMT_VAG, FMT_AT3, RAW_AT3, FMT_XMA1, RAW_XMA1, FMT_OGG, FMT_CWAV, FMT_APM, FMT_MPDX } ubi_sb_codec;
typedef enum { UBI_PC, UBI_PS2, UBI_XBOX, UBI_GC, UBI_X360, UBI_PSP, UBI_PS3, UBI_WII, UBI_3DS } ubi_sb_platform;
typedef enum { UBI_NONE = 0, UBI_AUDIO, UBI_LAYER, UBI_SEQUENCE, UBI_SILENCE } ubi_sb_type;

typedef struct {
    int map_version;
    size_t map_entry_size;
    size_t section1_entry_size;
    size_t section2_entry_size;
    size_t section3_entry_size;
    size_t resource_name_size;

    off_t audio_extra_offset;
    off_t audio_stream_size;
    off_t audio_stream_offset;
    off_t audio_stream_type;
    off_t audio_group_id;
    off_t audio_external_flag;
    off_t audio_loop_flag;
    off_t audio_num_samples;
    off_t audio_num_samples2;
    off_t audio_sample_rate;
    off_t audio_channels;
    off_t audio_stream_name;
    off_t audio_extra_name;
    off_t audio_xma_offset;
    int audio_external_and;
    int audio_loop_and;
    int audio_group_and;
    int audio_has_internal_names;
    size_t audio_interleave;
    int audio_fix_psx_samples;

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
    size_t layer_entry_size;
    size_t layer_hijack;

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
    int default_codec_for_group0;
} ubi_sb_config;

typedef struct {
    ubi_sb_platform platform;
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
    off_t map_num;

    uint32_t map_type;
    uint32_t map_zero;
    off_t map_offset;
    off_t map_size;
    char map_name[0x28];
    uint32_t map_unknown;

    /* SB info (some values are derived depending if it's standard sbX or map sbX) */
    int is_bank;
    int is_map;
    int is_bnm;
    uint32_t version;           /* 16b+16b major+minor version */
    uint32_t version_empty;     /* map sbX versions are empty */
    /* events (often share header_id/type with some descriptors,
     * but may exist without headers or header exist without them) */
    size_t section1_num;
    size_t section1_offset;
    /* descriptors, audio header or other config types */
    size_t section2_num;
    size_t section2_offset;
    /* internal streams table, referenced by each header */
    size_t section3_num;
    size_t section3_offset;
    /* section with sounds in some map versions */
    size_t section4_num;
    size_t section4_offset;
    /* extra table, config for certain types (DSP coefs, external resources, layer headers, etc) */
    size_t sectionX_size;
    size_t sectionX_offset;
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
    uint32_t group_id;          /* internal id to reference in section3 */

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

    int is_external;            /* stream is in a external file */
    char resource_name[0x28];   /* filename to the external stream, or internal stream info for some games */

    char readable_name[255];    /* final subsong name */
    int types[16];              /* counts each header types, for debugging */
    int allowed_types[16];
} ubi_sb_header;

static int parse_bnm_header(ubi_sb_header * sb, STREAMFILE *streamFile);
static int parse_header(ubi_sb_header * sb, STREAMFILE *streamFile, off_t offset, int index);
static int parse_sb(ubi_sb_header * sb, STREAMFILE *streamFile, int target_subsong);
static VGMSTREAM * init_vgmstream_ubi_sb_header(ubi_sb_header *sb, STREAMFILE* streamTest, STREAMFILE *streamFile);
static int config_sb_platform(ubi_sb_header * sb, STREAMFILE *streamFile);
static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile);


/* .SBx - banks from Ubisoft's DARE (Digital Audio Rendering Engine) engine games in ~2000-2008+ */
VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE *streamFile) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = {0};
    int target_subsong = streamFile->stream_index;


    /* checks (number represents the platform, see later) */
    if (!check_extensions(streamFile, "sb0,sb1,sb2,sb3,sb4,sb5,sb6,sb7"))
        goto fail;

    /* .sbX (sound bank) is a small multisong format (loaded in memory?) that contains SFX data
     * but can also reference .ss0/ls0 (sound stream) external files for longer streams.
     * A companion .sp0 (sound project) describes files and if it uses BANKs (.sbX) or MAPs (.smX). */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, streamFile))
        goto fail;
    read_32bit = sb.big_endian ? read_32bitBE : read_32bitLE;

    if (target_subsong <= 0) target_subsong = 1;

    /* use smaller header buffer for performance */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;


    /* SB HEADER */
    /* SBx layout: header, section1, section2, extra section, section3, data (all except header can be null) */
    sb.is_bank = 1;
    sb.version       = read_32bit(0x00, streamFile);

    if (!config_sb_version(&sb, streamFile))
        goto fail;

    sb.section1_num  = read_32bit(0x04, streamFile);
    sb.section2_num  = read_32bit(0x08, streamFile);
    sb.section3_num  = read_32bit(0x0c, streamFile);
    sb.sectionX_size = read_32bit(0x10, streamFile);
    sb.flag1         = read_32bit(0x14, streamFile);

    if (sb.version <= 0x000A0000) {
        sb.section1_offset = 0x18;
    } else {
        sb.section1_offset = 0x1c;
        sb.flag2 = read_32bit(0x18, streamFile);
    }

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

    if (!parse_sb(&sb, streamTest, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, streamTest, streamFile);
    close_streamfile(streamTest);
    return vgmstream;

fail:
    close_streamfile(streamTest);
    return NULL;
}

/* .SMx - maps (sets of custom SBx files) also from Ubisoft's sound engine games in ~2000-2008+ */
VGMSTREAM * init_vgmstream_ubi_sm(STREAMFILE *streamFile) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *streamTest = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    ubi_sb_header sb = {0}, target_sb = {0};
    int target_subsong = streamFile->stream_index;
    int i;


    /* checks (number represents platform, lmX are localized variations) */
    if (!check_extensions(streamFile, "sm0,sm1,sm2,sm3,sm4,sm5,sm6,sm7,lm0,lm1,lm2,lm3,lm4,lm5,lm6,lm7"))
        goto fail;

    /* .smX (sound map) is a set of slightly different sbX files, compiled into one "map" file.
     * Map has a sbX (called "submap") per named area (example: menu, level1, boss1, level2...).
     * This counts subsongs from all sbX, so totals can be massive, but there are splitters into mini-smX. */


    /* PLATFORM DETECTION */
    if (!config_sb_platform(&sb, streamFile))
        goto fail;
    read_32bit = sb.big_endian ? read_32bitBE : read_32bitLE;

    if (target_subsong <= 0) target_subsong = 1;

    /* use smaller header buffer for performance */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;


    /* SM BASE HEADER */
    /* SMx layout: header with N map area offset/sizes + custom SBx with relative offsets */
    sb.is_map = 1;
    sb.version   = read_32bit(0x00, streamFile);
    sb.map_start = read_32bit(0x04, streamFile);
    sb.map_num   = read_32bit(0x08, streamFile);

    if (!config_sb_version(&sb, streamFile))
        goto fail;


    for (i = 0; i < sb.map_num; i++) {
        off_t offset = sb.map_start + i * sb.cfg.map_entry_size;

        /* SUBMAP HEADER */
        sb.map_type     = read_32bit(offset + 0x00, streamFile); /* usually 0/1=first, 0=rest */
        sb.map_zero     = read_32bit(offset + 0x04, streamFile);
        sb.map_offset   = read_32bit(offset + 0x08, streamFile);
        sb.map_size     = read_32bit(offset + 0x0c, streamFile); /* includes sbX header, but not internal streams */
        read_string(sb.map_name, sizeof(sb.map_name), offset + 0x10, streamFile); /* null-terminated and may contain garbage after null */
        if (sb.cfg.map_version >= 3)
            sb.map_unknown  = read_32bit(offset + 0x30, streamFile); /* uncommon, id/config? longer name? mem garbage? */

        /* SB HEADER */
        /* SBx layout: base header, section1, section2, section4, extra section, section3, data (all except header can be null?) */
        sb.version_empty    = read_32bit(sb.map_offset + 0x00, streamFile); /* sbX in maps don't set version */
        sb.section1_offset  = read_32bit(sb.map_offset + 0x04, streamFile) + sb.map_offset;
        sb.section1_num     = read_32bit(sb.map_offset + 0x08, streamFile);
        sb.section2_offset  = read_32bit(sb.map_offset + 0x0c, streamFile) + sb.map_offset;
        sb.section2_num     = read_32bit(sb.map_offset + 0x10, streamFile);

        if (sb.cfg.map_version < 3) {
            sb.section3_offset  = read_32bit(sb.map_offset + 0x14, streamFile) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x20, streamFile);
        } else {
            sb.section4_offset  = read_32bit(sb.map_offset + 0x14, streamFile);
            sb.section4_num     = read_32bit(sb.map_offset + 0x18, streamFile);
            sb.section3_offset  = read_32bit(sb.map_offset + 0x1c, streamFile) + sb.map_offset;
            sb.section3_num     = read_32bit(sb.map_offset + 0x20, streamFile);
            sb.sectionX_offset  = read_32bit(sb.map_offset + 0x24, streamFile) + sb.map_offset;
            sb.sectionX_size    = read_32bit(sb.map_offset + 0x28, streamFile);

            /* latest map format has another section with sounds after section 2 */
            sb.section2_num    += sb.section4_num;    /* let's just merge it with section 2 */
            sb.sectionX_offset += sb.section4_offset; /* for some reason, this is relative to section 4 here */
        }

        VGM_ASSERT(sb.map_type != 0 && sb.map_type != 1, "UBI SM: unknown map_type at %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.map_zero != 0, "UBI SM: unknown map_zero at %x\n", (uint32_t)offset);
        //;VGM_ASSERT(sb.map_unknown != 0, "UBI SM: unknown map_unknown at %x\n", (uint32_t)offset);
        VGM_ASSERT(sb.version_empty != 0, "UBI SM: unknown version_empty at %x\n", (uint32_t)offset);

        if (!parse_sb(&sb, streamTest, target_subsong))
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
    vgmstream = init_vgmstream_ubi_sb_header(&target_sb, streamTest, streamFile);
    close_streamfile(streamTest);
    return vgmstream;

fail:
    close_streamfile(streamTest);
    return NULL;
}


/* .BNM - proto-sbX with map style format [Rayman 2 (PC), Donald Duck: Goin' Quackers (PC), Tonic Trouble (PC)] */
VGMSTREAM * init_vgmstream_ubi_bnm(STREAMFILE *streamFile) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *streamTest = NULL;
    ubi_sb_header sb = {0};
    int target_subsong = streamFile->stream_index;

    if (target_subsong <= 0) target_subsong = 1;


    /* checks */
    if (!check_extensions(streamFile, "bnm"))
        goto fail;

    /* v0, header is somewhat like a map-style bank (offsets + sizes) but sectionX/3 fields are
     * fixed/reserved (unused?). Header entry sizes and config works the same, and type numbers are
     * slightly different, but otherwise pretty much the same engine (not named DARE yet). Curiously
     * it may stream RIFF .wav (stream_offset pointing to "data"), and also .raw (PCM) or .apm IMA. */

    /* use smaller header buffer for performance */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;

    if (!parse_bnm_header(&sb, streamTest))
        goto fail;

    if (!parse_sb(&sb, streamTest, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_header(&sb, streamTest, streamFile);
    close_streamfile(streamTest);
    return vgmstream;

fail:
    close_streamfile(streamTest);
    return NULL;
}

static int parse_bnm_header(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;

    /* PLATFORM DETECTION */
    sb->platform = UBI_PC;
    read_32bit = sb->big_endian ? read_32bitBE : read_32bitLE;

    /* SB HEADER */
    /* SBx layout: header, section1, section2, extra section, section3, data (all except header can be null) */
    sb->is_bnm = 1;
    sb->version          = read_32bit(0x00, streamFile);
    sb->section1_offset  = read_32bit(0x04, streamFile);
    sb->section1_num     = read_32bit(0x08, streamFile);
    sb->section2_offset  = read_32bit(0x0c, streamFile);
    sb->section2_num     = read_32bit(0x10, streamFile);
    /* next are data start offset x3 + data size offset x3 */
    sb->section3_offset  = read_32bit(0x14, streamFile);
    sb->section3_num     = 0;

    if (!config_sb_version(sb, streamFile))
        goto fail;

    sb->sectionX_offset  = sb->section2_offset + sb->section2_num * sb->cfg.section2_entry_size;
    sb->sectionX_size    = sb->section3_offset - sb->sectionX_offset;

    return 1;
fail:
    return 0;
}

static int is_bnm_other_bank(STREAMFILE *streamFile, int bank_number) {
    char current_name[PATH_LIMIT];
    char bank_name[255];

    get_streamfile_filename(streamFile, current_name, PATH_LIMIT);
    sprintf(bank_name, "Bnk_%i.bnm", bank_number);

    return strcmp(current_name, bank_name) != 0;
}

#if 0
/* .BLK - maps in separate .blk chunks [Donald Duck: Goin' Quackers (PS2), The Jungle Book Rhythm N'Groove (PS2)] */
VGMSTREAM * init_vgmstream_ubi_blk(STREAMFILE *streamFile) {

    /* Somewhat equivalent to a v0x00000003 map:
     * - HEADER.BLK: base map header (slightly different?) + submaps headers
     * - EVT.BLK: section1 from all submaps
     * - RES.BLK: section2 + sectionX from all submaps
     * - MAPS.BLK, MAPLANG.BLK: section3 variation?
     * - STREAMED.BLK, STRLANG.BLK: audio data
     *
     * Parsing may be be simplified with multifile_streamfiles?
     */
    return NULL;
}
#endif

/* ************************************************************************* */

static VGMSTREAM * init_vgmstream_ubi_sb_base(ubi_sb_header *sb, STREAMFILE *streamHead, STREAMFILE *streamData, off_t start_offset) {
    VGMSTREAM * vgmstream = NULL;


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

        case UBI_ADPCM:
            /* custom Ubi 4/6-bit ADPCM used in early games:
             * - Splinter Cell (PC): 4-bit w/ 2ch (music), 6-bit w/ 1ch (sfx)
             * - Batman: Vengeance (PC): 4-bit w/ 2ch (music), 6-bit w/ 1ch (sfx)
             * - Myst IV (PC/Xbox): 4bit-1ch (amb), some files only (ex. sfx_si_puzzle_stream.sb2)
             * - possibly others */

            /* skip extra header (some kind of id?) found in Myst IV */
            if (read_32bitBE(start_offset + 0x00, streamData) != 0x08000000 &&
                read_32bitBE(start_offset + 0x08, streamData) == 0x08000000) {
                start_offset += 0x08;
                sb->stream_size -= 0x08;
            }

            vgmstream->codec_data = init_ubi_adpcm(streamData, start_offset, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_UBI_ADPCM;
            vgmstream->layout_type = layout_none;
            break;

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            if (vgmstream->num_samples == 0) { /* happens in .bnm */
                //todo with external wav streams stream_size may be off?
                vgmstream->num_samples       = pcm_bytes_to_samples(sb->stream_size, sb->channels, 16);
                vgmstream->loop_end_sample   = vgmstream->num_samples;
            }
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size =  (sb->cfg.audio_interleave) ?
                            sb->cfg.audio_interleave :
                            sb->stream_size / sb->channels;

            if (vgmstream->num_samples == 0) { /* early PS2 games may not set it for internal streams */
                vgmstream->num_samples = ps_bytes_to_samples(sb->stream_size, sb->channels);
                vgmstream->loop_end_sample = vgmstream->num_samples;
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
            dsp_read_coefs_be(vgmstream, streamHead, sb->extra_offset + 0x10, 0x40);
            dsp_read_hist_be (vgmstream, streamHead, sb->extra_offset + 0x34, 0x40); /* after gain/initial ps */
            break;

        case FMT_VAG:
            /* skip VAG header (some sb4 use VAG and others raw PSX) */
            if (read_32bitBE(start_offset, streamData) == 0x56414770) { /* "VAGp" */
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
            if (read_32bitBE(start_offset+0x04,streamData) == 0x52494646) {
                VGM_LOG("UBI SB: skipping unknown value 0x%x before RIFF\n", read_32bitBE(start_offset+0x00,streamData));
                start_offset += 0x04;
                sb->stream_size -= 0x04;
            }

            vgmstream->codec_data = init_ffmpeg_atrac3_riff(streamData, start_offset, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case RAW_AT3: {
            int block_align, encoder_delay;

            block_align = 0x98 * sb->channels;
            encoder_delay = 0; /* TODO: this is may be incorrect */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamData, start_offset,sb->stream_size, sb->num_samples,sb->channels,sb->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        //todo: some XMA1 decode a bit strangely at certain positions (FFmpeg bug?)
        case FMT_XMA1: {
            ffmpeg_codec_data *ffmpeg_data;
            uint8_t buf[0x100];
            uint32_t sec1_num, sec2_num, sec3_num, bits_per_frame;
            uint8_t flag;
            size_t bytes, chunk_size, header_size, data_size;
            off_t header_offset;

            chunk_size = 0x20;

            /* formatted XMA sounds have a strange custom header */
            header_offset = start_offset; /* XMA fmt chunk at the start */
            flag = read_8bit(header_offset + 0x20, streamData);
            sec2_num = read_32bitBE(header_offset + 0x24, streamData); /* number of XMA frames */
            sec1_num = read_32bitBE(header_offset + 0x28, streamData);
            sec3_num = read_32bitBE(header_offset + 0x2c, streamData);

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

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, data_size, streamData, 1);

            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_ch(vgmstream, streamData, start_offset, data_size, sb->channels, 0, 0);
            break;
        }

        case RAW_XMA1: {
            ffmpeg_codec_data *ffmpeg_data;
            uint8_t buf[0x100];
            size_t bytes, chunk_size;
            off_t header_offset;

            VGM_ASSERT(sb->is_external, "Ubi SB: Raw XMA used for external sound\n");

            /* get XMA header from extra section */
            chunk_size = 0x20;
            header_offset = sb->xma_header_offset;
            if (header_offset == 0)
                header_offset = sb->extra_offset;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, sb->stream_size, streamHead, 1);

            ffmpeg_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, sb->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_ch(vgmstream, streamData, start_offset, sb->stream_size, sb->channels, 0, 0);
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, sb->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        case FMT_CWAV:
            if (sb->channels > 1) goto fail; /* unknown layout */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            dsp_read_coefs_le(vgmstream,streamData,start_offset + 0x7c, 0x40);
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
                    vgmstream->ch[i].adpcm_history1_32 = read_32bitLE(start_offset + 0x2c + 0x0c*(sb->channels - 1 - i) + 0x00, streamData);
                    vgmstream->ch[i].adpcm_step_index  = read_32bitLE(start_offset + 0x2c + 0x0c*(sb->channels - 1 - i) + 0x04, streamData);
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

    /* open the actual for decoding (streamData can be an internal or external stream) */
    if ( !vgmstream_open_stream(vgmstream, streamData, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_sb_audio(ubi_sb_header *sb, STREAMFILE *streamTest, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *streamData = NULL;

    /* open external stream if needed */
    if (sb->is_external) {
        streamData = open_streamfile_by_filename(streamFile,sb->resource_name);
        if (streamData == NULL) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb->resource_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }


    /* init actual VGMSTREAM */
    vgmstream = init_vgmstream_ubi_sb_base(sb, streamTest, streamData, sb->stream_offset);
    if (!vgmstream) goto fail;


    if (sb->is_external && streamData) close_streamfile(streamData);
    return vgmstream;

fail:
    if (sb->is_external && streamData) close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_sb_layer(ubi_sb_header *sb, STREAMFILE *streamTest, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    STREAMFILE *streamData = NULL;
    size_t full_stream_size = sb->stream_size;
    int i, total_channels = 0;

    /* open external stream if needed */
    if (sb->is_external) {
        streamData = open_streamfile_by_filename(streamFile,sb->resource_name);
        if (streamData == NULL) {
            VGM_LOG("UBI SB: external stream '%s' not found\n", sb->resource_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }

    /* init layout */
    data = init_layout_layered(sb->layer_count);
    if (!data) goto fail;

    /* open all layers and mix */
    for (i = 0; i < sb->layer_count; i++) {
        /* prepare streamfile from a single layer section */
        temp_streamFile = setup_ubi_sb_streamfile(streamData, sb->stream_offset, full_stream_size, i, sb->layer_count, sb->big_endian, sb->cfg.layer_hijack);
        if (!temp_streamFile) goto fail;

        sb->stream_size = get_streamfile_size(temp_streamFile);
        sb->channels = sb->layer_channels[i];
        total_channels += sb->layer_channels[i];

        /* build the layer VGMSTREAM (standard sb with custom streamfile) */
        data->layers[i] = init_vgmstream_ubi_sb_base(sb, streamTest, temp_streamFile, 0x00);
        if (!data->layers[i]) goto fail;

        close_streamfile(temp_streamFile);
        temp_streamFile = NULL;
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

    if (sb->is_external && streamData) close_streamfile(streamData);

    return vgmstream;
fail:
    close_streamfile(temp_streamFile);
    if (sb->is_external && streamData) close_streamfile(streamData);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_layered(data);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_sb_sequence(ubi_sb_header *sb, STREAMFILE *streamTest, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    segmented_layout_data* data = NULL;
    int i;
    STREAMFILE *streamBank = streamTest;


    //todo optimization: open streamData once / only if new name (doesn't change 99% of the time)

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
        if (sb->is_bnm) {
            /* see if *current* bank has changed (may use a different bank N times) */
            if (is_bnm_other_bank(streamBank, sb->sequence_banks[i])) {
                char bank_name[255];
                sprintf(bank_name, "Bnk_%i.bnm", sb->sequence_banks[i]);

                if (streamBank != streamTest)
                    close_streamfile(streamBank);

                streamBank = open_streamfile_by_filename(streamFile, bank_name);
                if (!streamBank) goto fail;
            }

            /* re-parse the thing */
            if (!parse_bnm_header(&temp_sb, streamBank))
                goto fail;
            temp_sb.total_subsongs = 1; /* eh... just to keep parse_header happy */
        }
        else {
            temp_sb = *sb;  /* memcpy'ed */
        }

        /* parse expected entry */
        entry_offset = temp_sb.section2_offset + temp_sb.cfg.section2_entry_size * entry_index;
        if (!parse_header(&temp_sb, streamBank, entry_offset, entry_index))
            goto fail;

        if (temp_sb.type == UBI_NONE || temp_sb.type == UBI_SEQUENCE) {
            VGM_LOG("UBI SB: unexpected sequence %i entry type at %x\n", i, (uint32_t)entry_offset);
            goto fail; /* not seen, technically ok but too much recursiveness? */
        }

        /* build the layer VGMSTREAM (current sb entry config) */
        data->segments[i] = init_vgmstream_ubi_sb_header(&temp_sb, streamBank, streamFile);
        if (!data->segments[i]) goto fail;

        if (i == sb->sequence_loop)
            sb->loop_start = sb->num_samples;
        sb->num_samples += data->segments[i]->num_samples;

        /* save current (silences don't have values, so this ensures they know later, when memcpy'ed) */
        sb->channels = temp_sb.channels;
        sb->sample_rate = temp_sb.sample_rate;
    }

    if (streamBank != streamTest)
        close_streamfile(streamBank);

    if (!setup_layout_segmented(data))
        goto fail;

    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(data->segments[0]->channels, !sb->sequence_single);
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
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_segmented(data);
    if (streamBank != streamTest)
        close_streamfile(streamBank);
    return NULL;
}

static size_t silence_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, void* data) {
    int i;
    for (i = 0; i < length; i++) {
        dest[i] = 0;
    }
    return length; /* pretend we read zeroes */
}
static size_t silence_io_size(STREAMFILE *streamfile, void* data) {
    return 0x7FFFFFF; /* whatevs */
}
static STREAMFILE* setup_silence_streamfile(STREAMFILE *streamFile) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, NULL,0, silence_io_read,silence_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_sb_silence(ubi_sb_header *sb, STREAMFILE *streamTest, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    int channel_count, sample_rate;

    channel_count = sb->channels;
    sample_rate = sb->sample_rate;

    /* by default silences don't have settings so let's pretend */
    if (channel_count == 0)
        channel_count = 2;
    if (sample_rate == 0)
        sample_rate = 48000;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = (int32_t)(sb->duration * (float)sample_rate);
    vgmstream->num_streams = sb->total_subsongs;
    vgmstream->stream_size = vgmstream->num_samples * channel_count * 0x02; /* PCM size */

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    temp_streamFile = setup_silence_streamfile(streamFile);
    if ( !vgmstream_open_stream(vgmstream, temp_streamFile, 0x00) )
        goto fail;
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(temp_streamFile);
    return vgmstream;
}


static VGMSTREAM * init_vgmstream_ubi_sb_header(ubi_sb_header *sb, STREAMFILE* streamTest, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;

    if (sb->total_subsongs == 0) {
        VGM_LOG("UBI SB: no subsongs\n");
        goto fail;
    }

    ;VGM_LOG("UBI SB: target at %x + %x, extra=%x, name=%s, g=%i, t=%i\n",
        (uint32_t)sb->header_offset, sb->cfg.section2_entry_size, (uint32_t)sb->extra_offset, sb->resource_name, sb->group_id, sb->stream_type);
    ;VGM_LOG("UBI SB: stream offset=%x, size=%x, name=%s\n", (uint32_t)sb->stream_offset, sb->stream_size, sb->is_external ? sb->resource_name : "internal" );

    switch(sb->type) {
        case UBI_AUDIO:
            vgmstream = init_vgmstream_ubi_sb_audio(sb, streamTest, streamFile);
            break;

        case UBI_LAYER:
            vgmstream = init_vgmstream_ubi_sb_layer(sb, streamTest, streamFile);
            break;

        case UBI_SEQUENCE:
            vgmstream = init_vgmstream_ubi_sb_sequence(sb, streamTest, streamFile);
            break;

        case UBI_SILENCE:
            vgmstream = init_vgmstream_ubi_sb_silence(sb, streamTest, streamFile);
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

static void build_readable_name(char * buf, size_t buf_size, ubi_sb_header * sb) {
    const char *grp_name;
    const char *res_name;
    uint32_t id;
    uint32_t type;
    int index;

    /* config */
    if (sb->is_map) {
        grp_name = sb->map_name;
    }
    else if (sb->is_bnm) {
        if (sb->sequence_multibank)
            grp_name = "bnm-multi";
        else
            grp_name = "bnm";
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

static int parse_type_audio(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;

    /* audio header */
    sb->type = UBI_AUDIO;

    sb->extra_offset    = read_32bit(offset + sb->cfg.audio_extra_offset, streamFile) + sb->sectionX_offset;
    sb->stream_size     = read_32bit(offset + sb->cfg.audio_stream_size, streamFile);
    sb->stream_offset   = read_32bit(offset + sb->cfg.audio_stream_offset, streamFile);
    sb->channels        = (sb->cfg.audio_channels % 4) ? /* non-aligned offset is always 16b */
                (uint16_t)read_16bit(offset + sb->cfg.audio_channels, streamFile) :
                (uint32_t)read_32bit(offset + sb->cfg.audio_channels, streamFile);
    sb->sample_rate     = read_32bit(offset + sb->cfg.audio_sample_rate, streamFile);
    sb->stream_type     = read_32bit(offset + sb->cfg.audio_stream_type, streamFile);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    if (sb->cfg.audio_external_flag) {
        sb->is_external = (read_32bit(offset + sb->cfg.audio_external_flag, streamFile) & sb->cfg.audio_external_and);
    }

    if (sb->cfg.audio_group_id) {
        sb->group_id   = read_32bit(offset + sb->cfg.audio_group_id, streamFile);
        if (sb->cfg.audio_group_and) sb->group_id  &= sb->cfg.audio_group_and;

        /* normalize bitflag, known groups are only id 0/1 (if needed could calculate
         * shift-right value here, based on cfg.audio_group_and first 1-bit) */
        if (sb->group_id > 1)
            sb->group_id = 1;
    }

    if (sb->cfg.audio_loop_flag) {
        sb->loop_flag = (read_32bit(offset + sb->cfg.audio_loop_flag, streamFile) & sb->cfg.audio_loop_and);
    }

    if (sb->loop_flag) {
        sb->loop_start  = read_32bit(offset + sb->cfg.audio_num_samples, streamFile);
        sb->num_samples = read_32bit(offset + sb->cfg.audio_num_samples2, streamFile) + sb->loop_start;

        if (sb->cfg.audio_num_samples == sb->cfg.audio_num_samples2) { /* early games just repeat and don't set loop start */
            sb->num_samples = sb->loop_start;
            sb->loop_start = 0;
        }
        /* Loop starts that aren't 0 do exist but are very rare (ex. Splinter Cell PC, Beowulf PSP sb 82, index 575).
         * Also rare are looping external streams, since it's normally done through sequences (ex. Surf's Up).
         * Loop end may be +1? (ex. Splinter Cell: Double Agent PS3 #14331). */
    } else {
        sb->num_samples = read_32bit(offset + sb->cfg.audio_num_samples, streamFile);
    }

    if (sb->cfg.resource_name_size > sizeof(sb->resource_name)) {
        goto fail;
    }

    /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
    if (sb->cfg.audio_stream_name) {
        read_string(sb->resource_name, sb->cfg.resource_name_size, offset + sb->cfg.audio_stream_name, streamFile);
    }
    else {
        sb->cfg.audio_stream_name = read_32bit(offset + sb->cfg.audio_extra_name, streamFile);
        if (sb->cfg.layer_stream_name != 0xFFFFFFFF)
            read_string(sb->resource_name, sb->cfg.resource_name_size, sb->sectionX_offset + sb->cfg.audio_stream_name, streamFile);
    }

    /* points at XMA1 header in the extra section (only for RAW_XMA1, ignored garbage otherwise) */
    if (sb->cfg.audio_xma_offset) {
        sb->xma_header_offset = read_32bit(offset + sb->cfg.audio_xma_offset, streamFile) + sb->sectionX_offset;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_sequence(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    off_t table_offset;
    int i;

    /* sequence chain */
    sb->type = UBI_SEQUENCE;
    if (sb->cfg.sequence_entry_size == 0) {
        VGM_LOG("Ubi SB: sequence entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    sb->extra_offset    = read_32bit(offset + sb->cfg.sequence_extra_offset, streamFile) + sb->sectionX_offset;
    sb->sequence_loop   = read_32bit(offset + sb->cfg.sequence_sequence_loop, streamFile);
    sb->sequence_single = read_32bit(offset + sb->cfg.sequence_sequence_single, streamFile);
    sb->sequence_count  = read_32bit(offset + sb->cfg.sequence_sequence_count, streamFile);

    if (sb->sequence_count > SB_MAX_CHAIN_COUNT) {
        VGM_LOG("Ubi SB: incorrect sequence count %i vs %i\n", sb->sequence_count, SB_MAX_CHAIN_COUNT);
        goto fail;
    }

    /* get chain in extra table */
    table_offset = sb->extra_offset;
    for (i = 0; i < sb->sequence_count; i++) {
        uint32_t entry_number = (uint32_t)read_32bit(table_offset+sb->cfg.sequence_entry_number, streamFile);

        /* bnm sequences may refer to entries from different banks, whee */
        if (sb->is_bnm) {
            int16_t bank_number = (entry_number >> 16) & 0xFFFF;
            entry_number        = (entry_number >> 00) & 0xFFFF;

            //;VGM_LOG("UBI SB: bnm sequence entry=%i, bank=%i\n", entry_number, bank_number);
            sb->sequence_banks[i] = bank_number;

            /* info flag, does bank number point to another file? */
            if (!sb->sequence_multibank) {
                sb->sequence_multibank = is_bnm_other_bank(streamFile, bank_number);
            }
        }
        else {
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

static int parse_type_layer(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sb->big_endian ? read_16bitBE : read_16bitLE;
    off_t table_offset;
    int i;

    /* layer header */
    sb->type = UBI_LAYER;
    if (sb->cfg.layer_entry_size == 0) {
        VGM_LOG("Ubi SB: layer entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    sb->extra_offset    = read_32bit(offset + sb->cfg.layer_extra_offset, streamFile) + sb->sectionX_offset;
    sb->layer_count     = read_32bit(offset + sb->cfg.layer_layer_count, streamFile);
    sb->stream_size     = read_32bit(offset + sb->cfg.layer_stream_size, streamFile);
    sb->stream_offset   = read_32bit(offset + sb->cfg.layer_stream_offset, streamFile);

    if (sb->stream_size == 0) {
        VGM_LOG("UBI SB: bad stream size\n");
        goto fail;
    }

    if (sb->layer_count > SB_MAX_LAYER_COUNT) {
        VGM_LOG("Ubi SB: incorrect layer count\n");
        goto fail;
    }

    /* get 1st layer header in extra table and validate all headers match */
    table_offset = sb->extra_offset;
  //sb->channels        = (sb->cfg.layer_channels % 4) ? /* non-aligned offset is always 16b */
  //            (uint16_t)read_16bit(table_offset + sb->cfg.layer_channels, streamFile) :
  //            (uint32_t)read_32bit(table_offset + sb->cfg.layer_channels, streamFile);
    sb->sample_rate     = read_32bit(table_offset + sb->cfg.layer_sample_rate, streamFile);
    sb->stream_type     = read_32bit(table_offset + sb->cfg.layer_stream_type, streamFile);
    sb->num_samples     = read_32bit(table_offset + sb->cfg.layer_num_samples, streamFile);

    for (i = 0; i < sb->layer_count; i++) {
        int channels    = (sb->cfg.layer_channels % 4) ? /* non-aligned offset is always 16b */
                (uint16_t)read_16bit(table_offset + sb->cfg.layer_channels, streamFile) :
                (uint32_t)read_32bit(table_offset + sb->cfg.layer_channels, streamFile);
        int sample_rate = read_32bit(table_offset + sb->cfg.layer_sample_rate, streamFile);
        int stream_type = read_32bit(table_offset + sb->cfg.layer_stream_type, streamFile);
        int num_samples = read_32bit(table_offset + sb->cfg.layer_num_samples, streamFile);

        if (sb->sample_rate != sample_rate || sb->stream_type != stream_type) {
            VGM_LOG("Ubi SB: %i layer headers don't match at %x > %x\n", sb->layer_count, (uint32_t)offset, (uint32_t)table_offset);
            if (!sb->cfg.ignore_layer_error) /* layers of different rates happens sometimes */
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

    /* all layers seem external */
    sb->is_external = 1;

    /* external stream name can be found in the header (first versions) or the sectionX table (later versions) */
    if (sb->cfg.layer_stream_name) {
        read_string(sb->resource_name, sb->cfg.resource_name_size, offset + sb->cfg.layer_stream_name, streamFile);
    } else {
        sb->cfg.layer_stream_name = read_32bit(offset + sb->cfg.layer_extra_name, streamFile);
        if (sb->cfg.layer_stream_name != 0xFFFFFFFF)
            read_string(sb->resource_name, sb->cfg.resource_name_size, sb->sectionX_offset + sb->cfg.layer_stream_name, streamFile);
    }

    /* layers seem to include XMA header */

    return 1;
fail:
    return 0;
}

static int parse_type_silence(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t duration_int;
    float* duration_float;

    /* silence header */
    sb->type = UBI_SILENCE;
    if (sb->cfg.silence_duration_int == 0 && sb->cfg.silence_duration_float == 0) {
        VGM_LOG("Ubi SB: silence duration not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    if (sb->cfg.silence_duration_int) {
        duration_int = (uint32_t)read_32bit(offset + sb->cfg.silence_duration_int, streamFile);
        sb->duration = (float)duration_int / 65536.0f; /* 65536.0 is common so probably means 1.0 */
    }
    else if (sb->cfg.silence_duration_float) {
        duration_int = (uint32_t)read_32bit(offset + sb->cfg.silence_duration_float, streamFile);
        duration_float = (float*)&duration_int;
        sb->duration = *duration_float;
    }

    return 1;
fail:
    return 0;
}

// todo improve, only used in bnm sequences as sequence end (and may point to another bnm)
static int parse_type_random(ubi_sb_header * sb, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    off_t sb_extra_offset, table_offset;
    int i, sb_sequence_count;

    /* sequence chain */
    if (sb->cfg.random_entry_size == 0) {
        VGM_LOG("Ubi SB: random entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    sb_extra_offset    = read_32bit(offset + sb->cfg.random_extra_offset, streamFile) + sb->sectionX_offset;
    sb_sequence_count  = read_32bit(offset + sb->cfg.random_sequence_count, streamFile);


    /* get chain in extra table */
    table_offset = sb_extra_offset;
    for (i = 0; i < sb_sequence_count; i++) {
        uint32_t entry_number = (uint32_t)read_32bit(table_offset+0x00, streamFile);
        //uint32_t entry_chance = (uint32_t)read_32bit(table_offset+0x04, streamFile);

        if (sb->is_bnm) {
            int16_t bank_number = (entry_number >> 16) & 0xFFFF;
            entry_number        = (entry_number >> 00) & 0xFFFF;

            ;VGM_LOG("UBI SB: bnm sequence entry=%i, bank=%i\n", entry_number, bank_number);
            //sb->sequence_banks[i] = bank_number;

            /* not seen */
            if (is_bnm_other_bank(streamFile, bank_number)) {
                VGM_LOG("UBI SB: random in other bank\n");
                goto fail;
            }
        }

        //todo make rand or stuff (old chance: int from 0 to 0x10000, new: float from 0.0 to 1.0)
        { //if (entry_chance == ...)
            off_t entry_offset = sb->section2_offset + sb->cfg.section2_entry_size * entry_number;
            return parse_type_audio(sb, entry_offset, streamFile);
        }

        table_offset += sb->cfg.random_entry_size;
    }

    return 1;
fail:
    return 0;
}


/* find actual codec from type (as different games' stream_type can overlap) */
static int parse_stream_codec(ubi_sb_header * sb) {

    if (sb->type == UBI_SEQUENCE)
        return 1;

    if (sb->cfg.default_codec_for_group0 && sb->type == UBI_AUDIO && sb->group_id == 0) {
        /* early Xbox games contain garbage in stream_type field in this case, it seems that 0x00 is assumed */
        sb->stream_type = 0x00;
    }

    /* guess codec */
    switch (sb->stream_type) {
        case 0x00: /* platform default (rarely external) */
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
#if 0
                case UBI_PS3: /* assumed, but no games seem to use it */
                    sb->codec = RAW_AT3;
                    break;
#endif
                case UBI_3DS:
                    sb->codec = FMT_CWAV;
                    break;
                default:
                    VGM_LOG("UBI SB: unknown internal format\n");
                    goto fail;
            }
            break;

        case 0x01:
            switch (sb->version) {
                case 0x00000003: /* Donald Duck: Goin' Quackers */
                case 0x00000004: /* Myst III: Exile */
                case 0x00000007: /* Splinter Cell */
                case 0x0000000D: /* Prince of Persia: The Sands of Time Demo */
                    switch (sb->platform) {
                        case UBI_PS2:   sb->codec = RAW_PSX; break;
                        case UBI_GC:    sb->codec = RAW_DSP; break;
                        case UBI_PC:    sb->codec = RAW_PCM; break;
                        case UBI_XBOX:  sb->codec = RAW_XBOX; break;
                        default: VGM_LOG("UBI SB: unknown old internal format\n"); goto fail;
                    }
                    break;
                default:
                    sb->codec = RAW_PCM; /* uncommon, ex. Wii/PSP/3DS */
                    break;
            }
            break;

        case 0x02:
            switch (sb->version) {
                case 0x00000000: /* Tonic Trouble Special Edition */
                    sb->codec = FMT_MPDX;
                    break;
                case 0x00000007: /* Splinter Cell, Splinter Cell: Pandora Tomorrow */
                case 0x0000000D: /* Prince of Persia: The Sands of Time Demo */
                case 0x000A0000:
                case 0x000A0002:
                case 0x00120012: /* Myst IV: Exile */
                    //todo splinter Cell Essentials
                    sb->codec = UBI_ADPCM;
                    break;
                default:
                    sb->codec = RAW_PSX; /* PS3 */
                    break;
            }
            break;

        case 0x03:
            sb->codec = UBI_IMA; /* Ubi IMA v3+ (versions handled in decoder) */
            break;

        case 0x04:
            switch (sb->version) {
                case 0x00000000: /* Rayman 2, Tonic Trouble */
                    sb->codec = FMT_APM;
                    break;
                case 0x00000007: /* Splinter Cell, Splinter Cell: Pandora Tomorrow */
                    sb->codec = UBI_IMA;
                    break;
                default:
                    sb->codec = FMT_OGG; /* later PC games */
                    break;
            }
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
                    VGM_LOG("UBI SB: unknown codec for stream_type %x\n", sb->stream_type);
                    goto fail;
            }
            break;

        case 0x06:
            switch (sb->version) {
                case 0x00000003: /* Batman: Vengeance (PC) */
                    sb->codec = UBI_ADPCM;
                    break;
                default:
                    sb->codec = RAW_PSX; /* later PSP and PS3(?) games */
                    break;
            }
            break;

        case 0x07:
            sb->codec = RAW_AT3; /* PS3 games */
            break;

        case 0x08:
            switch (sb->version) {
                case 0x00000003: /* Donald Duck: Goin' Quackers */
                case 0x00000004: /* Myst III: Exile */
                    sb->codec = UBI_IMA; /* Ubi IMA v2/v3 */
                    break;
                default:
                    sb->codec = FMT_AT3;
                    break;
            }
            break;

        default:
            VGM_LOG("UBI SB: unknown stream_type %x\n", sb->stream_type);
            goto fail;
    }

    return 1;
fail:
    return 0;
}

/* find actual stream offset in section3 */
static int parse_offsets(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;
    int i, j, k;

    if (sb->type == UBI_SEQUENCE)
        return 1;

    VGM_ASSERT(!sb->is_map && sb->section3_num > 2, "UBI SB: section3 > 2 found\n");

    if (!(sb->cfg.audio_group_id || sb->is_map) && sb->section3_num > 1) {
        VGM_LOG("UBI SB: unexpected number of internal stream groups %i\n", sb->section3_num);
        goto fail;
    }

    if (sb->is_external)
        return 1;

    /* Internal sounds are split into codec groups, with their offsets being relative to group start.
     * A table contains sizes of each group, so we adjust offsets based on the group ID of our sound.
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
         * 0x00: sec2 entry index
         * 0x04: sound offset
         * table 2 - for each entry:
         * 0x00 - group ID
         * 0x04 - size with padding included
         * 0x08 - size without padding
         * 0x0c - absolute group offset */

        for (i = 0; i < sb->section3_num; i++) {
            off_t offset = sb->section3_offset + 0x14 * i;
            off_t table_offset  = read_32bit(offset + 0x04, streamFile) + sb->section3_offset;
            uint32_t table_num  = read_32bit(offset + 0x08, streamFile);
            off_t table2_offset = read_32bit(offset + 0x0c, streamFile) + sb->section3_offset;
            uint32_t table2_num = read_32bit(offset + 0x10, streamFile);

            for (j = 0; j < table_num; j++) {
                int index = read_32bit(table_offset + 0x08 * j + 0x00, streamFile) & 0x0000FFFF;

                if (index == sb->header_index) {
                    if (!sb->cfg.audio_group_id && table2_num > 1) {
                        VGM_LOG("UBI SB: unexpected number of internal stream map groups %i at %x\n", table2_num, (uint32_t)table2_offset);
                        goto fail;
                    }

                    sb->stream_offset = read_32bit(table_offset + 0x08 * j + 0x04, streamFile);
                    for (k = 0; k < table2_num; k++) {
                        uint32_t id = read_32bit(table2_offset + 0x10 * k + 0x00, streamFile);

                        if (id == sb->group_id) {
                            sb->stream_offset += read_32bit(table2_offset + 0x10 * k + 0x0c, streamFile);
                            break;
                        }
                    }
                    break;
                }
            }

            if (sb->stream_offset)
                break;
        }
    }
    else {
        /* banks store internal sounds after all headers and adjusted by the group table, find the matching entry */

        off_t sounds_offset = sb->section3_offset + sb->cfg.section3_entry_size*sb->section3_num;
        if (sb->cfg.is_padded_sounds_offset)
            sounds_offset = align_size_to_block(sounds_offset, 0x10);
        sb->stream_offset = sounds_offset + sb->stream_offset;

        if (sb->cfg.audio_group_id && sb->section3_num > 1) { /* maybe should always test this? */
            for (i = 0; i < sb->section3_num; i++) {
                off_t offset = sb->section3_offset + sb->cfg.section3_entry_size * i;

                /* table has unordered ids+size, so if our id doesn't match current data offset must be beyond */
                if (read_32bit(offset + 0x00, streamFile) == sb->group_id)
                    break;
                sb->stream_offset += read_32bit(offset + 0x04, streamFile);
            }
        }
    }

    return 1;
fail:
    return 0;
}

/* parse a single known header resource at offset (see config_sb for info) */
static int parse_header(ubi_sb_header * sb, STREAMFILE *streamFile, off_t offset, int index) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sb->big_endian ? read_32bitBE : read_32bitLE;

    sb->header_index    = index;
    sb->header_offset   = offset;

    sb->header_id       = read_32bit(offset + 0x00, streamFile);
    sb->header_type     = read_32bit(offset + 0x04, streamFile);

    switch(sb->header_type) {
        case 0x01:
            if (!parse_type_audio(sb, offset, streamFile))
                goto fail;
            break;
        case 0x05:
        case 0x0b:
        case 0x0c:
            if (!parse_type_sequence(sb, offset, streamFile))
                goto fail;
            break;
        case 0x06:
        case 0x0d:
            if (!parse_type_layer(sb, offset, streamFile))
                goto fail;
            break;
        case 0x08:
        case 0x0f:
            if (!parse_type_silence(sb, offset, streamFile))
                goto fail;
            break;
        case 0x0a:
            if (!parse_type_random(sb, offset, streamFile))
                goto fail;
            break;
        default:
            VGM_LOG("UBI SB: unknown header type %x at %x\n", sb->header_type, (uint32_t)offset);
            goto fail;
    }

    if (!parse_stream_codec(sb))
        goto fail;

    if (!parse_offsets(sb, streamFile))
        goto fail;

    return 1;
fail:
    return 0;
}

/* parse a bank and its possible audio headers */
static int parse_sb(ubi_sb_header * sb, STREAMFILE *streamFile, int target_subsong) {
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

      /*header_id =*/ read_32bit(offset + 0x00, streamFile); /* forces buffer read */
        header_type = read_32bit(offset + 0x04, streamFile);

        if (header_type <= 0x00 || header_type >= 0x10) {
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

        if (!parse_header(sb, streamFile, offset, i))
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

static int config_sb_platform(ubi_sb_header * sb, STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    int filename_len;
    char platform_char;
    uint32_t version;

    /* to find out hijacking (LE) platforms */
    version = read_32bitLE(0x00, streamFile);

    /* get X from .sbX/smX/lmX */
    get_streamfile_name(streamFile,filename,sizeof(filename));
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


static void config_sb_entry(ubi_sb_header * sb, size_t section1_size_entry, size_t section2_size_entry) {
    sb->cfg.section1_entry_size     = section1_size_entry;
    sb->cfg.section2_entry_size     = section2_size_entry;
    sb->cfg.section3_entry_size     = 0x08;
}
static void config_sb_audio_fs(ubi_sb_header * sb, off_t external_flag, off_t group_id, off_t loop_flag) {
    /* audio header with standard flags */
    sb->cfg.audio_external_flag     = external_flag;
    sb->cfg.audio_group_id          = group_id;
    sb->cfg.audio_loop_flag         = loop_flag;
    sb->cfg.audio_external_and      = 1;
    sb->cfg.audio_group_and         = 1;
    sb->cfg.audio_loop_and          = 1;
}
static void config_sb_audio_fb(ubi_sb_header * sb, off_t flag_bits, int external_and, int group_and, int loop_and) {
    /* audio header with bit flags */
    sb->cfg.audio_external_flag     = flag_bits;
    sb->cfg.audio_group_id          = flag_bits;
    sb->cfg.audio_loop_flag         = flag_bits;
    sb->cfg.audio_external_and      = external_and;
    sb->cfg.audio_group_and         = group_and;
    sb->cfg.audio_loop_and          = loop_and;
}
static void config_sb_audio_hs(ubi_sb_header * sb, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t stream_name, off_t stream_type) {
    /* audio header with stream name */
    sb->cfg.audio_channels          = channels;
    sb->cfg.audio_sample_rate       = sample_rate;
    sb->cfg.audio_num_samples       = num_samples;
    sb->cfg.audio_num_samples2      = num_samples2;
    sb->cfg.audio_stream_name       = stream_name;
    sb->cfg.audio_stream_type       = stream_type;
}
static void config_sb_audio_he(ubi_sb_header * sb, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t extra_name, off_t stream_type) {
    /* audio header with extra name */
    sb->cfg.audio_channels          = channels;
    sb->cfg.audio_sample_rate       = sample_rate;
    sb->cfg.audio_num_samples       = num_samples;
    sb->cfg.audio_num_samples2      = num_samples2;
    sb->cfg.audio_extra_name        = extra_name;
    sb->cfg.audio_stream_type       = stream_type;
}
static void config_sb_sequence(ubi_sb_header * sb, off_t sequence_count, off_t entry_size) {
    /* sequence header and chain table */
    sb->cfg.sequence_sequence_loop  = sequence_count - 0x10;
    sb->cfg.sequence_sequence_single= sequence_count - 0x0c;
    sb->cfg.sequence_sequence_count = sequence_count;
    sb->cfg.sequence_entry_size     = entry_size;
    sb->cfg.sequence_entry_number   = 0x00;
    if (sb->is_bnm) {
        sb->cfg.sequence_sequence_loop  = sequence_count - 0x0c;
        sb->cfg.sequence_sequence_single= sequence_count - 0x08;
    }
}
static void config_sb_layer_hs(ubi_sb_header * sb, off_t layer_count, off_t stream_size, off_t stream_offset, off_t stream_name) {
    /* layer headers with stream name */
    sb->cfg.layer_layer_count       = layer_count;
    sb->cfg.layer_stream_size       = stream_size;
    sb->cfg.layer_stream_offset     = stream_offset;
    sb->cfg.layer_stream_name       = stream_name;
}
static void config_sb_layer_he(ubi_sb_header * sb, off_t layer_count, off_t stream_size, off_t stream_offset, off_t extra_name) {
    /* layer headers with extra name */
    sb->cfg.layer_layer_count       = layer_count;
    sb->cfg.layer_stream_size       = stream_size;
    sb->cfg.layer_stream_offset     = stream_offset;
    sb->cfg.layer_extra_name        = extra_name;
}
static void config_sb_layer_sh(ubi_sb_header * sb, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples) {
    /* layer sub-headers in extra table */
    sb->cfg.layer_entry_size        = entry_size;
    sb->cfg.layer_sample_rate       = sample_rate;
    sb->cfg.layer_channels          = channels;
    sb->cfg.layer_stream_type       = stream_type;
    sb->cfg.layer_num_samples       = num_samples;
}
static void config_sb_silence_i(ubi_sb_header * sb, off_t duration) {
    /* silence headers in int value */
    sb->cfg.silence_duration_int    = duration;
}
static void config_sb_silence_f(ubi_sb_header * sb, off_t duration) {
    /* silence headers in float value */
    sb->cfg.silence_duration_float  = duration;
}

static void config_sb_random_old(ubi_sb_header * sb, off_t sequence_count, off_t entry_size) {
    sb->cfg.random_sequence_count = sequence_count;
    sb->cfg.random_entry_size = entry_size;
    sb->cfg.random_percent_int = 1;
}

static int config_sb_version(ubi_sb_header * sb, STREAMFILE *streamFile) {
    int is_dino_pc = 0;
    int is_ttse_pc = 0;
    int is_bia_ps2 = 0, is_biadd_psp = 0;
    int is_sc2_ps2_gc = 0;
    int is_sc4_pc_online = 0;

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

    if (sb->version == 0x00000000 || sb->version == 0x00000200) {
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
    }
    else if (sb->version <= 0x00000007) {
        sb->cfg.audio_stream_size       = 0x0c;
        sb->cfg.audio_extra_offset      = 0x10;
        sb->cfg.audio_stream_offset     = 0x14;

        sb->cfg.sequence_extra_offset   = 0x10;

        sb->cfg.layer_extra_offset      = 0x10;
    }
    else {
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
    if (sb->is_bnm) {
      //sb->allowed_types[0x0a] = 1; /* only needed inside sequences */
        sb->allowed_types[0x0b] = 1;
        sb->allowed_types[0x09] = 1;
    }

#if 0
    {
        STREAMFILE * streamTest;
        streamTest= open_streamfile_by_filename(streamFile, ".no_audio.sbx");
        if (streamTest) { sb->allowed_types[0x01] = 0; close_streamfile(streamTest); }

        streamTest= open_streamfile_by_filename(streamFile, ".no_sequence.sbx");
        if (streamTest) { sb->allowed_types[0x05] = sb->allowed_types[0x0c] = 0; close_streamfile(streamTest); }

        streamTest= open_streamfile_by_filename(streamFile, ".no_layer.sbx");
        if (streamTest) { sb->allowed_types[0x06] = sb->allowed_types[0x0d] = 0; close_streamfile(streamTest); }
    }
#endif

    /* two configs with same id; use SND file as identifier */
    if (sb->version == 0x00000000 && sb->platform == UBI_PC) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "Dino.lcb");
        if (streamTest) {
            is_dino_pc = 1;
            close_streamfile(streamTest);
        }
    }

    /* some files in Dinosaur */
    if (sb->version == 0x00000200 && sb->platform == UBI_PC) {
        sb->version = 0x00000000;
        is_dino_pc = 1;
    }

    /* Tonic Touble beta has garbage instead of version */
    if (sb->is_bnm && sb->version > 0x00000000 && sb->platform == UBI_PC) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "ED_MAIN.LCB");
        if (streamTest) {
            is_ttse_pc = 1;
            sb->version = 0x00000000;
            close_streamfile(streamTest);
        }
    }


    /* Tonic Trouble Special Edition (1999)(PC)-bnm */
    if (sb->version == 0x00000000 && sb->platform == UBI_PC && is_ttse_pc) {
        config_sb_entry(sb, 0x20, 0x5c);

        config_sb_audio_fs(sb, 0x2c, 0x2c, 0x30); /* no group id */
        config_sb_audio_hs(sb, 0x42, 0x3c, 0x38, 0x38, 0x48, 0x44);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x24, 0x18);

        //config_sb_random_old(sb, 0x18, 0x0c);

        /* no layers */
        //todo type 9 needed
        //todo MPX don't set stream size?
        return 1;
    }


    /* Rayman 2: The Great Escape (1999)(PC)-bnm */
    /* Tonic Trouble (1999)(PC)-bnm */
    /* Donald Duck: Goin' Quackers (2000)(PC)-bnm */
    /* Disney's Dinosaur (2000)(PC)-bnm */
    if (sb->version == 0x00000000 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x20, 0x5c);

        config_sb_audio_fs(sb, 0x2c, 0x2c, 0x30); /* no group id */
        config_sb_audio_hs(sb, 0x42, 0x3c, 0x34, 0x34, 0x48, 0x44);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x24, 0x18);

        config_sb_random_old(sb, 0x18, 0x0c); /* Rayman 2 needs it for rare sequence ends (ex. Bnk_31.bnm) */

        /* no layers */

        if (is_dino_pc)
            config_sb_entry(sb, 0x20, 0x60);
        return 1;
    }

    /* Batman: Vengeance (2001)(PC)-map */
    /* Batman: Vengeance (2001)(Xbox)-map */
    if ((sb->version == 0x00000003 && sb->platform == UBI_PC) ||
        (sb->version == 0x00000003 && sb->platform == UBI_XBOX)) {
        config_sb_entry(sb, 0x40, 0x68);

        config_sb_audio_fs(sb, 0x30, 0x30, 0x34); /* no group id? use external flag */
        config_sb_audio_hs(sb, 0x52, 0x4c, 0x38, 0x40, 0x58, 0x54);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x2c, 0x1c);

        config_sb_layer_hs(sb, 0x20, 0x4c, 0x44, 0x34);
        config_sb_layer_sh(sb, 0x1c, 0x04, 0x0a, 0x0c, 0x18);
        return 1;
    }

    /* Disney's Tarzan: Untamed (2001)(GC)-map */
    /* Batman: Vengeance (2001)(GC)-map */
    /* Donald Duck: Goin' Quackers (2002)(GC)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x40, 0x6c);

        config_sb_audio_fs(sb, 0x30, 0x30, 0x34); /* no group id? use external flag */
        config_sb_audio_hs(sb, 0x56, 0x50, 0x48, 0x48, 0x5c, 0x58); /* 0x38 may be num samples too? */

        config_sb_sequence(sb, 0x2c, 0x1c);

        config_sb_layer_hs(sb, 0x20, 0x4c, 0x44, 0x34);
        config_sb_layer_sh(sb, 0x1c, 0x04, 0x0a, 0x0c, 0x18);
        return 1;
    }

#if 0
    //todo too weird
    /* Batman: Vengeance (2001)(PS2)-map */
    /* Disney's Tarzan: Untamed (2001)(PS2)-map */
    if (sb->version == 0x00000003 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x30, 0x3c);

        config_sb_audio_fb(sb, 0x1c, (1 << 2), (1 << 3), (1 << 4)); /* not ok */
        config_sb_audio_hs(sb, 0x00, 0x24, 0x28, 0x28, 0x00, 0x00);
        /* channels: 0? maybe 2=external, 1=internal? */
        /* stream type: always PS-ADPCM (interleave unknown) */
        /* sb->cfg.audio_stream_string = "STRM.SM1"; */ /* fixed */

        config_sb_sequence(sb, 0x2c, 0x10); /* this is normal enough */

        /* layers have a weird format too */
        return 1;
    }
#endif

    //todo group flags and maybe num_samples for sfx are off
    /* Myst III: Exile (2001)(PS2)-map */
    if (sb->version == 0x00000004 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x34, 0x70);

        config_sb_audio_fb(sb, 0x1c, (1 << 3), (1 << 6), (1 << 4)); //???
        config_sb_audio_hs(sb, 0x24, 0x28, 0x2c, 0x34, 0x44, 0x6c);
        sb->cfg.audio_external_flag = 0x6c; /* no external flag? use codec as flag */

        config_sb_sequence(sb, 0x2c, 0x24);
        return 1;
    }

    /* Splinter Cell (2002)(PC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(PC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x58, 0x80);

        config_sb_audio_fs(sb, 0x28, 0x28, 0x2c); /* no group id? use external flag */
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

        config_sb_audio_fs(sb, 0x28, 0x28, 0x2c); /* no group id? use external flag */
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
        is_sc2_ps2_gc = read_32bit(0x08, streamFile) == 0x21;

        /* could also load ECHELON.SP1/Echelon.SP3 and test BE 0x04 == 0x00ACBF77,
         * but it's worse for localization subdirs without it */
    }

    /* Splinter Cell (2002)(PS2)-map */
    /* Splinter Cell: Pandora Tomorrow (2006)(PS2)-map 0x00000007 */
    if (sb->version == 0x00000007 && sb->platform == UBI_PS2) {
        config_sb_entry(sb, 0x40, 0x70);

        config_sb_audio_fb(sb, 0x1c, (1 << 2), (1 << 3), (1 << 4));
        config_sb_audio_hs(sb, 0x24, 0x28, 0x34, 0x3c, 0x44, 0x6c); /* num_samples may be null */

        config_sb_sequence(sb, 0x2c, 0x30);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);

        if (is_sc2_ps2_gc) {
            sb->cfg.map_entry_size = 0x38;
            /* some amb .ss2 have bad sizes with mixed random data, bad extraction/unused crap? */
            /* Pandora Tomorrow voices have bad offsets too */
        }
        return 1;
    }

    /* Splinter Cell (2002)(GC)-map */
    /* Splinter Cell: Pandora Tomorrow (2004)(GC)-map */
    if (sb->version == 0x00000007 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x58, 0x78);

        config_sb_audio_fs(sb, 0x24, 0x24, 0x28); /* no group id? use external flag */
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x2c, 0x34, 0x50, 0x4c);

        config_sb_sequence(sb, 0x2c, 0x34);

        config_sb_layer_hs(sb, 0x24, 0x64, 0x5c, 0x34);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x06, 0x08, 0x14);

        if (is_sc2_ps2_gc) {
            sb->cfg.map_entry_size = 0x38;
            sb->cfg.audio_external_and = 0x01000000; /* did somebody forget about BE? */
        }
        return 1;
    }

    /* Prince of Persia: The Sands of Time Demo (2003)(Xbox)-bank 0x0000000D */
    if (sb->version == 0x0000000D && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x5c, 0x74);

        config_sb_audio_fs(sb, 0x24, 0x24, 0x28); /* no group id? use external flag */
        config_sb_audio_hs(sb, 0x46, 0x40, 0x2c, 0x34, 0x4c, 0x48);
        sb->cfg.audio_has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: The Sands of Time Demo (2003)(Xbox)-bank 0x000A0000 */
    if (sb->version == 0x000A0000 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x78);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x2c);
        config_sb_audio_hs(sb, 0x4a, 0x44, 0x30, 0x38, 0x50, 0x4c);

        config_sb_sequence(sb, 0x28, 0x14);

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
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP1");
        if (streamTest) {
            is_bia_ps2 = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: The Sands of Time (2003)(PS2)-bank 0x000A0004 / 0x000A0002 (POP1 port/Demo) */
    /* Tom Clancy's Rainbow Six 3 (2003)(PS2)-bank 0x000A0007 */
    /* Tom Clancy's Ghost Recon 2 (2004)(PS2)-bank 0x000A0007 */
    /* Splinter Cell: Pandora Tomorrow (2006)(PS2)-bank 0x000A0008 (separate banks from main map) */
    /* Prince of Persia: Warrior Within Demo (2004)(PS2)-bank 0x00100000 */
    /* Prince of Persia: Warrior Within (2004)(PS2)-bank 0x00120009 */
    if ((sb->version == 0x000A0002 && sb->platform == UBI_PS2) ||
        (sb->version == 0x000A0004 && sb->platform == UBI_PS2) ||
        (sb->version == 0x000A0007 && sb->platform == UBI_PS2 && !is_bia_ps2) ||
        (sb->version == 0x000A0008 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00100000 && sb->platform == UBI_PS2) ||
        (sb->version == 0x00120009 && sb->platform == UBI_PS2)) {
        config_sb_entry(sb, 0x48, 0x6c);

        config_sb_audio_fb(sb, 0x18, (1 << 2), (1 << 3), (1 << 4));
        config_sb_audio_hs(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x68); /* num_samples may be null */

        config_sb_sequence(sb, 0x28, 0x10);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        config_sb_silence_i(sb, 0x18);

        return 1;
    }

    /* Brothers in Arms: Road to Hill 30 (2005)[PS2] */
    /* Brothers in Arms: Earned in Blood (2005)[PS2] */
    if (sb->version == 0x000A0007 && sb->platform == UBI_PS2 && is_bia_ps2) {
        config_sb_entry(sb, 0x5c, 0x14c);

        config_sb_audio_fb(sb, 0x18, (1 << 2), (1 << 3), (1 << 4));
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

    /* Batman: Rise of Sin Tzu (2003)(Xbox)-map 0x000A0003 */
    if (sb->version == 0x000A0003 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x80);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x34);
        config_sb_audio_hs(sb, 0x52, 0x4c, 0x38, 0x40, 0x58, 0x54);
        sb->cfg.audio_has_internal_names = 1;
        sb->cfg.default_codec_for_group0 = 1;

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
        sb->cfg.audio_has_internal_names = 1;
        sb->cfg.default_codec_for_group0 = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);
        return 1;
    }

    /* Batman: Rise of Sin Tzu (2003)(GC)-map 0x000A0002 */
    /* Prince of Persia: The Sands of Time (2003)(GC)-bank 0x000A0004 / 0x000A0002 (POP1 port) */
    /* Tom Clancy's Rainbow Six 3 (2003)(Xbox)-bank 0x000A0007 */
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

    /* Tom Clancy's Rainbow Six 3 (2003)(Xbox)-bank 0x000A0007 */
    if (sb->version == 0x000A0007 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x64, 0x8c);

        config_sb_audio_fs(sb, 0x24, 0x28, 0x40);
        config_sb_audio_hs(sb, 0x5e, 0x58, 0x44, 0x4c, 0x64, 0x60);
        sb->cfg.audio_has_internal_names = 1;
        sb->cfg.default_codec_for_group0 = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x20, 0x60, 0x58, 0x30);
        config_sb_layer_sh(sb, 0x14, 0x00, 0x06, 0x08, 0x10);

        config_sb_silence_i(sb, 0x18);
        return 1;
    }

    /* Myst IV Demo (2004)(PC)-bank */
    if (sb->version == 0x00100000 && sb->platform == UBI_PC) {
        config_sb_entry(sb, 0x68, 0xa4);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;
        return 1;
    }

    /* Prince of Persia: Warrior Within Demo (2004)(PC)-bank 0x00120006 */
    /* Prince of Persia: Warrior Within (2004)(PC)-bank 0x00120009 */
    if ((sb->version == 0x00120006 && sb->platform == UBI_PC) ||
        (sb->version == 0x00120009 && sb->platform == UBI_PC)) {
        config_sb_entry(sb, 0x6c, 0x84);

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
        sb->cfg.default_codec_for_group0 = 1;

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
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "BIAAUDIO.SP4");
        if (streamTest) {
            is_biadd_psp = 1;
            close_streamfile(streamTest);
        }
    }

    /* Prince of Persia: Revelations (2005)(PSP)-bank */
    /* Splinter Cell: Essentials (2006)(PSP)-map */
    /* Beowulf: The Game (2007)(PSP)-map */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && !is_biadd_psp) {
        config_sb_entry(sb, 0x68, 0x84);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;

        config_sb_sequence(sb, 0x28, 0x14);

        config_sb_layer_hs(sb, 0x1c, 0x60, 0x64, 0x30);
        config_sb_layer_sh(sb, 0x18, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    //todo some .sbX have bad external stream offsets, but not all (ex. offset 0xE3641 but should be 0x0A26)
    /* Brothers in Arms: D-Day (2006)(PSP)-bank */
    if (sb->version == 0x0012000C && sb->platform == UBI_PSP && is_biadd_psp) {
        config_sb_entry(sb, 0x80, 0x94);

        config_sb_audio_fs(sb, 0x24, 0x2c, 0x28);
        config_sb_audio_hs(sb, 0x4c, 0x44, 0x30, 0x38, 0x54, 0x50);
        sb->cfg.audio_has_internal_names = 1;
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

    /* Myst IV: Revelation (2005)(PC)-bank */
    /* Splinter Cell: Chaos Theory (2005)(Xbox)-map */
    if (sb->version == 0x00120012 && sb->platform == UBI_XBOX) {
        config_sb_entry(sb, 0x48, 0x4c);

        config_sb_audio_fb(sb, 0x18, (1 << 3), (1 << 4), (1 << 10));
        config_sb_audio_he(sb, 0x38, 0x30, 0x1c, 0x24, 0x40, 0x3c);

        config_sb_sequence(sb, 0x28, 0x10);
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

        config_sb_audio_fb(sb, 0x18, (1 << 2), (1 << 3), (1 << 4));
        config_sb_audio_he(sb, 0x20, 0x24, 0x30, 0x38, 0x40, 0x4c);
        sb->cfg.audio_interleave = 0x8000;

        sb->cfg.is_padded_section1_offset = 1;
        sb->cfg.is_padded_sounds_offset = 1;
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

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 4));
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

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 4));
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
        sb->cfg.layer_hijack = 1; /* WTF!!! layer format different from other layers using same id!!! */
        return 1;
    }

    /* Open Season (2006)(PC)-map 0x00180003 */
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

    /* Open Season (2006)(Xbox)-map 0x00180003 */
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

    /* Open Season (2006)(GC)-map 0x00180003 */
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


    /* two configs with same id; use project file as identifier */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC) {
        STREAMFILE * streamTest = open_streamfile_by_filename(streamFile, "Sc4_online_SoundProject.SP0");
        if (streamTest) {
            is_sc4_pc_online = 1;
            close_streamfile(streamTest);
        }
    }

    /* Splinter Cell: Double Agent (2006)(PC)-map (offline) */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC && !is_sc4_pc_online) {
        config_sb_entry(sb, 0x68, 0x7c);

        config_sb_audio_fs(sb, 0x2c, 0x34, 0x30);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Splinter Cell: Double Agent (2006)(PC)-map (online) */
    if (sb->version == 0x00180006 && sb->platform == UBI_PC && is_sc4_pc_online) {
        config_sb_entry(sb, 0x68, 0x78);

        config_sb_audio_fs(sb, 0x2c, 0x34, 0x30);
        config_sb_audio_he(sb, 0x5c, 0x54, 0x40, 0x48, 0x64, 0x60);

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

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* Red Steel (2006)(Wii)-bank */
    if (sb->version == 0x00180006 && sb->platform == UBI_WII) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30);
        config_sb_audio_he(sb, 0x58, 0x50, 0x3c, 0x44, 0x60, 0x5c);

        config_sb_sequence(sb, 0x2c, 0x14);

        config_sb_layer_he(sb, 0x20, 0x38, 0x3c, 0x44);
        config_sb_layer_sh(sb, 0x34, 0x00, 0x08, 0x0c, 0x14);
        return 1;
    }

    /* TMNT (2007)(PSP)-map 0x00190001 */
    /* Surf's Up (2007)(PSP)-map 0x00190005 */
    if ((sb->version == 0x00190001 && sb->platform == UBI_PSP) ||
        (sb->version == 0x00190005 && sb->platform == UBI_PSP)) {
        config_sb_entry(sb, 0x48, 0x58);

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 4)); /* assumed group_flag */
        config_sb_audio_he(sb, 0x28, 0x2c, 0x34, 0x3c, 0x44, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);
        return 1;
    }

    /* TMNT (2007)(GC)-bank */
    if (sb->version == 0x00190002 && sb->platform == UBI_GC) {
        config_sb_entry(sb, 0x68, 0x6c);

        config_sb_audio_fs(sb, 0x28, 0x2c, 0x30); /* assumed groud_id */
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

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 4)); /* assumed group_flag */
        config_sb_audio_he(sb, 0x28, 0x2c, 0x34, 0x3c, 0x44, 0x48);

        config_sb_sequence(sb, 0x2c, 0x10);

        config_sb_layer_he(sb, 0x20, 0x2c, 0x30, 0x38);
        config_sb_layer_sh(sb, 0x30, 0x00, 0x04, 0x08, 0x10);

        config_sb_silence_f(sb, 0x1c);
        return 1;
    }

    /* TMNT (2007)(X360)-bank 0x00190002 */
    /* Prince of Persia: Rival Swords (2007)(Wii)-bank 0x00190003 */
    /* Rainbow Six Vegas (2007)(PS3)-bank 0x00190005 */
    /* Surf's Up (2007)(PS3)-bank 0x00190005 */
    /* Surf's Up (2007)(X360)-bank 0x00190005 */
    /* Splinter Cell: Double Agent (2007)(PS3)-map 0x00190005 */
    if ((sb->version == 0x00190002 && sb->platform == UBI_X360) ||
        (sb->version == 0x00190003 && sb->platform == UBI_WII) ||
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

    /* Rainbow Six Vegas 2 (2008)(PS3)-bank */
    /* Rainbow Six Vegas 2 (2008)(X360)-bank */
    if ((sb->version == 0x001C0000 && sb->platform == UBI_PS3) ||
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

        config_sb_audio_fb(sb, 0x20, (1 << 2), (1 << 3), (1 << 5)); /* assumed group_flag */
        config_sb_audio_he(sb, 0x28, 0x30, 0x38, 0x40, 0x48, 0x4c);
        return 1;
    }

    /* Splinter Cell Classic Trilogy HD (2011)(PS3)-map */
    if (sb->version == 0x001D0000 && sb->platform == UBI_PS3) {
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

    VGM_LOG("UBI SB: unknown SB/SM version+platform %08x\n", sb->version);
    return 0;
}
