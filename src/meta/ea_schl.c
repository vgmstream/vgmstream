#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ea_schl_streamfile.h"

/* header version */
#define EA_VERSION_NONE             -1
#define EA_VERSION_V0               0x00 /* ~early PC (when codec1 was used) */
#define EA_VERSION_V1               0x01 /* ~PC */
#define EA_VERSION_V2               0x02 /* ~PS1 */
#define EA_VERSION_V3               0x03 /* ~PS2 */

/* platform constants (unassigned values seem internal only) */
#define EA_PLATFORM_PC              0x00
#define EA_PLATFORM_PSX             0x01
#define EA_PLATFORM_N64             0x02
#define EA_PLATFORM_MAC             0x03
#define EA_PLATFORM_SAT             0x04
#define EA_PLATFORM_PS2             0x05
#define EA_PLATFORM_GC              0x06 /* also used on Wii */
#define EA_PLATFORM_XBOX            0x07
#define EA_PLATFORM_GENERIC         0x08 /* typically Wii/X360/PS3/videos */
#define EA_PLATFORM_X360            0x09
#define EA_PLATFORM_PSP             0x0A
//#define EA_PLATFORM_PC_EAAC       0x0B /* not used (sx.exe internal defs) */
//#define EA_PLATFORM_X360_EAAC     0x0C /* not used (sx.exe internal defs) */
//#define EA_PLATFORM_PSP_EAAC      0x0D /* not used (sx.exe internal defs) */
#define EA_PLATFORM_PS3             0x0E /* very rare [Need for Speed: Carbon (PS3)] */
//#define EA_PLATFORM_PS3_EAAC      0x0F
#define EA_PLATFORM_WII             0x10 /* not seen so far (sx.exe samples ok) */
//#define EA_PLATFORM_WII_EAAC      0x11 /* not used (sx.exe internal defs) */
//#define EA_PLATFORM_PC64_EAAC     0x12 /* not used (sx.exe internal defs) */
//#define EA_PLATFORM_MOBILE_EAAC   0x13 /* not used (sx.exe internal defs) */
#define EA_PLATFORM_3DS             0x14

/* codec constants (undefined are probably reserved, ie.- sx.exe encodes PCM24/DVI but no platform decodes them) */
/* CODEC1 values were used early, then they migrated to CODEC2 values */
#define EA_CODEC1_NONE              -1
#define EA_CODEC1_PCM               0x00
//#define EA_CODEC1_IMA             0x02 /* not used (sx.exe internal defs) */
#define EA_CODEC1_N64               0x05
#define EA_CODEC1_VAG               0x06
#define EA_CODEC1_EAXA              0x07
#define EA_CODEC1_MT10              0x09

#define EA_CODEC2_NONE              -1
#define EA_CODEC2_S16LE_INT         0x00
#define EA_CODEC2_S16BE_INT         0x01
#define EA_CODEC2_S8_INT            0x02
#define EA_CODEC2_EAXA_INT          0x03
#define EA_CODEC2_MT10              0x04
#define EA_CODEC2_VAG               0x05
#define EA_CODEC2_N64               0x06
#define EA_CODEC2_S16BE             0x07
#define EA_CODEC2_S16LE             0x08
#define EA_CODEC2_S8                0x09
#define EA_CODEC2_EAXA              0x0A
//#define EA_CODEC2_U8_INT          0x0B /* not used (sx.exe internal defs) */
//#define EA_CODEC2_CDXA            0x0C /* not used (sx.exe internal defs) */
//#define EA_CODEC2_IMA_INT         0x0D /* not used (sx.exe internal defs) */
//#define EA_CODEC2_LAYER1          0x0E /* not used (sx.exe internal defs) */
#define EA_CODEC2_LAYER2            0x0F
#define EA_CODEC2_LAYER3            0x10 /* not seen so far but may be used somewhere */
#define EA_CODEC2_GCADPCM           0x12
//#define EA_CODEC2_S24LE_INT       0x13 /* not used (sx.exe internal defs) */
#define EA_CODEC2_XBOXADPCM         0x14
//#define EA_CODEC2_S24BE_INT       0x15 /* not used (sx.exe internal defs) */
#define EA_CODEC2_MT5               0x16
#define EA_CODEC2_EALAYER3          0x17
//#define EA_CODEC2_XAS0_INT        0x18 /* not used (sx.exe internal defs) */
//#define EA_CODEC2_EALAYER3_INT    0x19 /* not used (sx.exe internal defs) */
#define EA_CODEC2_ATRAC3            0x1A /* not seen so far (sx.exe samples ok) */
#define EA_CODEC2_ATRAC3PLUS        0x1B
/* EAAC (SND10) codecs begin after this point */

/* Block headers, SCxy - where x is block ID and y is endianness flag (always 'l'?) */
#define EA_BLOCKID_HEADER           0x5343486C /* "SCHl" */
#define EA_BLOCKID_COUNT            0x5343436C /* "SCCl" */
#define EA_BLOCKID_DATA             0x5343446C /* "SCDl" */
#define EA_BLOCKID_LOOP             0x53434C6C /* "SCLl */
#define EA_BLOCKID_END              0x5343456C /* "SCEl" */

/* Localized block headers, Sxyy - where x is block ID and yy is lang code (e.g. "SHEN"), used in videos */
#define EA_BLOCKID_LOC_HEADER       0x53480000 /* "SH" */
#define EA_BLOCKID_LOC_COUNT        0x53430000 /* "SC" */
#define EA_BLOCKID_LOC_DATA         0x53440000 /* "SD" */
#define EA_BLOCKID_LOC_END          0x53450000 /* "SE" */

#define EA_BLOCKID_LOC_EN           0x0000454E /* English */
#define EA_BLOCKID_LOC_FR           0x00004652 /* French */
#define EA_BLOCKID_LOC_GE           0x00004745 /* German, older */
#define EA_BLOCKID_LOC_DE           0x00004445 /* German, newer */
#define EA_BLOCKID_LOC_IT           0x00004954 /* Italian */
#define EA_BLOCKID_LOC_SP           0x00005350 /* Castilian Spanish, older */
#define EA_BLOCKID_LOC_ES           0x00004553 /* Castilian Spanish, newer */
#define EA_BLOCKID_LOC_MX           0x00004D58 /* Mexican Spanish */
#define EA_BLOCKID_LOC_RU           0x00005255 /* Russian */
#define EA_BLOCKID_LOC_JA           0x00004A41 /* Japanese, older */
#define EA_BLOCKID_LOC_JP           0x00004A50 /* Japanese, newer */
#define EA_BLOCKID_LOC_PL           0x0000504C /* Polish */
#define EA_BLOCKID_LOC_BR           0x00004252 /* Brazilian Portuguese */

#define EA_BNK_HEADER_LE            0x424E4B6C /* "BNKl" */
#define EA_BNK_HEADER_BE            0x424E4B62 /* "BNKb" */

#define EA_MAX_CHANNELS             6

typedef struct {
    int32_t num_samples;
    int32_t sample_rate;
    int32_t channels;
    int32_t platform;
    int32_t version;
    int32_t bps;
    int32_t codec1;
    int32_t codec2;

    int32_t loop_start;
    int32_t loop_end;

    uint32_t flag_value;

    off_t offsets[EA_MAX_CHANNELS];
    off_t coefs[EA_MAX_CHANNELS];
    off_t loops[EA_MAX_CHANNELS];

    int big_endian;
    int loop_flag;
    int codec_config;
    int use_pcm_blocks;

    size_t stream_size;
} ea_header;

static VGMSTREAM* parse_schl_block(STREAMFILE* sf, off_t offset);
static VGMSTREAM* parse_bnk_header(STREAMFILE* sf, off_t offset, int target_stream, int is_embedded);
static int parse_variable_header(STREAMFILE* sf, ea_header* ea, off_t begin_offset, int max_length, int bnk_version);
static uint32_t read_patch(STREAMFILE* sf, off_t* offset);
static off_t get_ea_stream_mpeg_start_offset(STREAMFILE* sf, off_t start_offset, const ea_header* ea);
static VGMSTREAM* init_vgmstream_ea_variable_header(STREAMFILE* sf, ea_header* ea, off_t start_offset, int is_bnk);
static void update_ea_stream_size(STREAMFILE* sf, off_t start_offset, VGMSTREAM* vgmstream);

/* EA SCHl with variable header - from EA games (roughly 1997~2010); generated by EA Canada's sx.exe/Sound eXchange */
VGMSTREAM* init_vgmstream_ea_schl(STREAMFILE* sf) {

    /* check extension */
    /* they don't seem enforced by EA's tools but usually:
     * .asf: ~early (audio stream file?) [ex. Need for Speed II (PC)]
     * .lasf: fake for plugins
     * .str: ~early [ex. FIFA 98 (PS1), FIFA 2002 (PS1)]
     * .chk: ~early [ex. NBA Live 98 (PS1)]
     * .eam: ~mid?
     * .exa: ~mid [ex. 007 - From Russia with Love]
     * .sng: ~late (FIFA games)
     * .aud: ~late [ex. FIFA 14 (3DS)]
     * .strm: MySims Kingdom (Wii)
     * .stm: FIFA 12 (3DS)
     * .sx: FIFA 98 (SAT)
     * .xa: ?
     * .hab: GoldenEye - Rogue Agent (inside .big)
     * .xsf: 007 - Agent Under Fire (Xbox)
     * .gsf: 007 - Everything or Nothing (GC)
     * (extensionless): SSX (PS2) (inside .big) */
    if (!check_extensions(sf,"asf,lasf,str,chk,eam,exa,sng,aud,sx,xa,strm,stm,hab,xsf,gsf,"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,sf) != EA_BLOCKID_HEADER &&  /* "SCHl" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_EN) && /* "SHEN" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_FR) && /* "SHFR" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_GE) && /* "SHGE" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_DE) && /* "SHDE" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_IT) && /* "SHIT" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_SP) && /* "SHSP" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_ES) && /* "SHES" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_MX) && /* "SHMX" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_RU) && /* "SHRU" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_JA) && /* "SHJA" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_JP) && /* "SHJP" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_PL) && /* "SHPL" */
        read_32bitBE(0x00,sf) != (EA_BLOCKID_LOC_HEADER | EA_BLOCKID_LOC_BR))   /* "SHBR" */
        goto fail;

    /* Stream is divided into blocks/chunks: SCHl=audio header, SCCl=count of SCDl, SCDl=data xN, SCLl=loop end, SCEl=end.
     * Video uses picture blocks (MVhd/MV0K/etc) and sometimes multiaudio blocks (SHxx/SCxx/SDxx/SExx where xx=language).
     * The number/size is affected by: block rate setting, sample rate, channels, CPU location (SPU/main/DSP/others), etc */
    return parse_schl_block(sf, 0x00);

fail:
    return NULL;
}

/* EA SCHl inside non-demuxed videos, used in current gen games too */
VGMSTREAM* init_vgmstream_ea_schl_video(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t offset = 0, start_offset = 0;
    int blocks_done = 0;
    int total_subsongs, target_subsong = sf->stream_index;
    int32_t(*read_32bit)(off_t, STREAMFILE*);


    /* check extension */
    /* .uv: early */
    /* .dct: early-mid [ex. Need for Speed II SE (PC), FIFA 98 (PC)] */
    /* .wve: early-mid [Madden NFL 99 (PC)] */
    /* .mad: mid */
    /* .vp6: late */
    if (check_extensions(sf, "uv,dct")) {
        /* starts with audio header block */
        if (read_32bitBE(0x00, sf) != EA_BLOCKID_HEADER) /* "SCHl" */
            goto fail;
    } else if (check_extensions(sf, "mad,wve")) {
        /* check initial movie block id */
        if (read_32bitBE(0x00, sf) != 0x4D41446B) /* "MADk" */
            goto fail;
    } else if (check_extensions(sf, "vp6")) {
        /* check initial movie block id */
        if (read_32bitBE(0x00, sf) != 0x4D566864) /* "MVhd" */
            goto fail;
    } else {
        goto fail;
    }

    /* use block size to check endianness */
    if (guess_endianness32bit(0x04, sf)) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    /* find starting valid header for the parser */
    while (offset < get_streamfile_size(sf)) {
        uint32_t block_id   = read_32bitBE(offset+0x00,sf);
        uint32_t block_size = read_32bit  (offset+0x04,sf);

        /* find "SCHl" or "SHxx" blocks */
        if ((block_id == EA_BLOCKID_HEADER) || ((block_id & 0xFFFF0000) == EA_BLOCKID_LOC_HEADER)) {
            start_offset = offset;
            break;
        }

        if (block_size == 0xFFFFFFFF)
            goto fail;
        if (blocks_done > 10)
            goto fail; /* unlikely to contain music */

        blocks_done++;
        offset += block_size;
    }

    if (offset >= get_streamfile_size(sf))
        goto fail;

    /* find target subsong (one per each SHxx multilang block) */
    total_subsongs = 1;
    if (target_subsong == 0) target_subsong = 1;
    offset = start_offset;
    while (offset < get_streamfile_size(sf)) {
        uint32_t block_id   = read_32bitBE(offset+0x00,sf);
        uint32_t block_size = read_32bit  (offset+0x04,sf);

        /* no more subsongs (assumes all SHxx headers go together) */
        if (((block_id & 0xFFFF0000) != EA_BLOCKID_LOC_HEADER)) {
            break;
        }

        if (target_subsong == total_subsongs) {
            start_offset = offset;
            /* keep counting subsongs */
        }

        total_subsongs++;
        offset += block_size;
    }

    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    vgmstream = parse_schl_block(sf, start_offset);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* EA BNK with variable header - from EA games SFXs; also created by sx.exe */
VGMSTREAM* init_vgmstream_ea_bnk(STREAMFILE* sf) {
    int target_stream = sf->stream_index;

    /* check extension */
    /* .bnk: common
     * .sdt: Harry Potter games
     * .abk: GoldenEye - Rogue Agent
     * .ast: FIFA 2004 (inside .big) */
    if (!check_extensions(sf,"bnk,sdt,abk,ast"))
        goto fail;

    if (target_stream == 0) target_stream = 1;
    return parse_bnk_header(sf, 0x00, target_stream - 1, 0);
    
fail:
    return NULL;
}

/* EA ABK - common soundbank format in 6th-gen games, can reference RAM and streamed assets */
/* RAM assets are stored in embedded BNK file */
/* streamed assets are stored externally in AST file (mostly seen in earlier 6th-gen games) */
VGMSTREAM* init_vgmstream_ea_abk(STREAMFILE* sf) {
    int bnk_target_stream, is_dupe, total_sounds = 0, target_stream = sf->stream_index;
    off_t bnk_offset, modules_table, module_data, player_offset, samples_table, entry_offset, target_entry_offset, schl_offset, schl_loop_offset;
    uint32_t i, j, k, num_sounds, num_sample_tables;
    uint16_t num_modules;
    uint8_t sound_type, num_players;
    off_t sample_tables[0x400];
    STREAMFILE * astData = NULL;
    VGMSTREAM * vgmstream = NULL;
    segmented_layout_data *data_s = NULL;
    int32_t(*read_32bit)(off_t, STREAMFILE*);
    int16_t(*read_16bit)(off_t, STREAMFILE*);

    /* check extension */
    if (!check_extensions(sf, "abk"))
        goto fail;

    if (read_32bitBE(0x00, sf) != 0x41424B43) /* "ABKC" */
        goto fail;

    /* use table offset to check endianness */
    if (guess_endianness32bit(0x1C, sf)) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    num_modules = read_16bit(0x0A, sf);
    modules_table = read_32bit(0x1C, sf);
    bnk_offset = read_32bit(0x20, sf);
    target_entry_offset = 0;
    num_sample_tables = 0;

    /* check to avoid clashing with the newer ABK format */
    if (bnk_offset &&
        read_32bitBE(bnk_offset, sf) != EA_BNK_HEADER_LE &&
        read_32bitBE(bnk_offset, sf) != EA_BNK_HEADER_BE)
        goto fail;

    for (i = 0; i < num_modules; i++) {
        num_players = read_8bit(modules_table + 0x24, sf);
        module_data = read_32bit(modules_table + 0x2C, sf);
        if (num_players == 0xff) goto fail; /* EOF read */

        for (j = 0; j < num_players; j++) {
            player_offset = read_32bit(modules_table + 0x3C + 0x04 * j, sf);
            samples_table = read_32bit(module_data + player_offset + 0x04, sf);

            /* multiple players may point at the same sound table */
            is_dupe = 0;
            for (k = 0; k < num_sample_tables; k++) {
                if (samples_table == sample_tables[k]) {
                    is_dupe = 1;
                    break;
                }
            }

            if (is_dupe)
                continue;

            sample_tables[num_sample_tables++] = samples_table;
            num_sounds = read_32bit(samples_table, sf);
            if (num_sounds == 0xffffffff) goto fail; /* EOF read */

            for (k = 0; k < num_sounds; k++) {
                entry_offset = samples_table + 0x04 + 0x0C * k;
                sound_type = read_8bit(entry_offset + 0x00, sf);

                /* some of these are dummies pointing at sound 0 in BNK */
                if (sound_type == 0x00 && read_32bit(entry_offset + 0x04, sf) == 0)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds)
                    target_entry_offset = entry_offset;
            }
        }

        /* skip class controllers */
        num_players += read_8bit(modules_table + 0x27, sf);
        modules_table += 0x3C + num_players * 0x04;
    }

    if (target_entry_offset == 0)
        goto fail;

    /* 0x00: type (0x00 - RAM, 0x01 - streamed, 0x02 - streamed looped) */
    /* 0x01: priority */
    /* 0x02: padding */
    /* 0x04: index for RAM sounds, offset for streamed sounds */
    /* 0x08: loop offset for streamed sounds */
    sound_type = read_8bit(target_entry_offset + 0x00, sf);

    switch (sound_type) {
        case 0x00:
            if (!bnk_offset)
                goto fail;

            bnk_target_stream = read_32bit(target_entry_offset + 0x04, sf);
            vgmstream = parse_bnk_header(sf, bnk_offset, bnk_target_stream, 1);
            if (!vgmstream)
                goto fail;

            break;

        case 0x01:
            astData = open_streamfile_by_ext(sf, "ast");
            if (!astData)
                goto fail;

            schl_offset = read_32bit(target_entry_offset + 0x04, sf);
            if (read_32bitBE(schl_offset, astData) != EA_BLOCKID_HEADER)
                goto fail;

            vgmstream = parse_schl_block(astData, schl_offset);
            if (!vgmstream)
                goto fail;

            break;

        case 0x02:
            astData = open_streamfile_by_ext(sf, "ast");
            if (!astData) {
                vgm_logi("EA ABK: .ast file not found (find and put together)\n");
                goto fail;
            }

            /* looped sounds basically consist of two independent segments
             * the first one is loop start, the second one is loop body */
            schl_offset = read_32bit(target_entry_offset + 0x04, sf);
            schl_loop_offset = read_32bit(target_entry_offset + 0x08, sf);

            if (read_32bitBE(schl_offset, astData) != EA_BLOCKID_HEADER ||
                read_32bitBE(schl_loop_offset, astData) != EA_BLOCKID_HEADER)
                goto fail;

            /* init layout */
            data_s = init_layout_segmented(2);
            if (!data_s) goto fail;

            /* load intro and loop segments */
            data_s->segments[0] = parse_schl_block(astData, schl_offset);
            if (!data_s->segments[0]) goto fail;
            data_s->segments[1] = parse_schl_block(astData, schl_loop_offset);
            if (!data_s->segments[1]) goto fail;

            /* setup segmented VGMSTREAMs */
            if (!setup_layout_segmented(data_s))
                goto fail;

            /* build the VGMSTREAM */
            vgmstream = allocate_segmented_vgmstream(data_s, 1, 1, 1);
            if (!vgmstream)
                goto fail;
            break;

        default:
            goto fail;
            break;
    }

    vgmstream->num_streams = total_sounds;
    close_streamfile(astData);
    return vgmstream;

fail:
    close_streamfile(astData);
    free_layout_segmented(data_s);
    return NULL;
}

/* EA HDR/DAT v1 (2004-2005) - used for storing speech, sometimes streamed SFX */
VGMSTREAM *init_vgmstream_ea_hdr_dat(STREAMFILE *sf) {
    VGMSTREAM *vgmstream;
    STREAMFILE *sf_dat = NULL, *temp_sf = NULL;
    int target_stream = sf->stream_index;
    uint32_t offset_mult, sound_offset, sound_size;
    uint8_t num_params, num_sounds;
    size_t dat_size;

    /* checks */
    if (!check_extensions(sf, "hdr"))
        goto fail;

    /* main header is machine endian but it's not important here */
    /* 0x00: ID */
    /* 0x02: speaker ID (used for different police voices in NFS games) */
    /* 0x04: number of parameters */
    /* 0x05: number of samples */
    /* 0x06: sample repeat (alt number of samples?) */
    /* 0x07: block size (offset multiplier) */
    /* 0x08: number of blocks (DAT size divided by block size) */
    /* 0x0a: number of sub-banks */
    /* 0x0c: table start */

    /* no nice way to validate these so we do what we can */
    if (read_u16be(0x0a, sf) != 0)
        goto fail;

    /* first offset is always zero */
    if (read_u16be(0x0c, sf) != 0)
        goto fail;

    /* must be accompanied by DAT file with SCHl or VAG sounds */
    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat)
        goto fail;

    if (read_u32be(0x00, sf_dat) != EA_BLOCKID_HEADER &&
        read_u32be(0x00, sf_dat) != 0x56414770)
        goto fail;

    num_params = read_u8(0x04, sf) & 0x7F;
    num_sounds = read_u8(0x05, sf);
    offset_mult = read_u8(0x07, sf) * 0x0100 + 0x0100;

    if (read_u8(0x06, sf) > num_sounds)
        goto fail;

    dat_size = get_streamfile_size(sf_dat);
    if (read_u16le(0x08, sf) * offset_mult > dat_size &&
        read_u16be(0x08, sf) * offset_mult > dat_size)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets are always big endian */
    sound_offset = read_u16be(0x0C + (0x02 + num_params) * (target_stream - 1), sf) * offset_mult;
    if (read_u32be(sound_offset, sf_dat) == EA_BLOCKID_HEADER) { /* "SCHl" */
        vgmstream = parse_schl_block(sf_dat, sound_offset);
        if (!vgmstream)
            goto fail;
    } else if (read_u32be(sound_offset, sf_dat) == 0x56414770) { /* "VAGp" */
        /* Need for Speed: Hot Pursuit 2 (PS2) */
        sound_size = read_u32be(sound_offset + 0x0c, sf_dat) + 0x30;
        temp_sf = setup_subfile_streamfile(sf_dat, sound_offset, sound_size, "vag");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_vag(temp_sf);
        if (!vgmstream) goto fail;
        close_streamfile(temp_sf);
    } else {
        goto fail;
    }

    if (num_params != 0) {
        uint8_t val;
        char buf[8];
        int i;
        for (i = 0; i < num_params; i++) {
            val = read_u8(0x0C + (0x02 + num_params) * (target_stream - 1) + 0x02 + i, sf);
            snprintf(buf, sizeof(buf), "%u", val);
            concatn(STREAM_NAME_SIZE, vgmstream->stream_name, buf);
            if (i != num_params - 1)
                concatn(STREAM_NAME_SIZE, vgmstream->stream_name, ", ");
        }
    }

    vgmstream->num_streams = num_sounds;
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_dat);
    close_streamfile(temp_sf);
    return NULL;
}

/* EA HDR/DAT v2 (2006-2014) */
VGMSTREAM* init_vgmstream_ea_hdr_dat_v2(STREAMFILE* sf) {
    VGMSTREAM *vgmstream;
    STREAMFILE *sf_dat = NULL;
    int target_stream = sf->stream_index;
    uint32_t offset_mult, sound_offset;
    uint8_t num_params, num_sounds;
    size_t dat_size;

    /* checks */
    if (!check_extensions(sf, "hdr"))
        goto fail;

    /* main header is machine endian but it's not important here */
    /* 0x00: ID */
    /* 0x02: number of parameters */
    /* 0x03: number of samples */
    /* 0x04: speaker ID (used for different police voices in NFS games) */
    /* 0x08: sample repeat (alt number of samples?) */
    /* 0x09: block size (offset multiplier) */
    /* 0x0a: number of blocks (DAT size divided by block size) */
    /* 0x0c: number of sub-banks (always zero?) */
    /* 0x0e: padding */
    /* 0x10: table start */

    /* no nice way to validate these so we do what we can */
    if (read_u32be(0x0c, sf) != 0)
        goto fail;

    /* first offset is always zero */
    if (read_u16be(0x10, sf) != 0)
        goto fail;

    /* must be accompanied by DAT file with SCHl sounds */
    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat)
        goto fail;

    if (read_u32be(0x00, sf_dat) != EA_BLOCKID_HEADER)
        goto fail;

    num_params = read_u8(0x02, sf) & 0x7F;
    num_sounds = read_u8(0x03, sf);
    offset_mult = read_u8(0x09, sf) * 0x0100 + 0x0100;

    if (read_u8(0x08, sf) > num_sounds)
        goto fail;

    dat_size = get_streamfile_size(sf_dat);
    if (read_u16le(0x0a, sf) * offset_mult > dat_size &&
        read_u16be(0x0a, sf) * offset_mult > dat_size)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets are always big endian */
    sound_offset = read_u16be(0x10 + (0x02 + num_params) * (target_stream - 1), sf) * offset_mult;
    if (read_u32be(sound_offset, sf_dat) != EA_BLOCKID_HEADER)
        goto fail;

    vgmstream = parse_schl_block(sf_dat, sound_offset);
    if (!vgmstream)
        goto fail;

    if (num_params != 0) {
        uint8_t val;
        char buf[8];
        int i;
        for (i = 0; i < num_params; i++) {
            val = read_u8(0x10 + (0x02 + num_params) * (target_stream - 1) + 0x02 + i, sf);
            snprintf(buf, sizeof(buf), "%u", val);
            concatn(STREAM_NAME_SIZE, vgmstream->stream_name, buf);
            if (i != num_params - 1)
                concatn(STREAM_NAME_SIZE, vgmstream->stream_name, ", ");
        }
    }

    vgmstream->num_streams = num_sounds;
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_dat);
    return NULL;
}


/* open map/mpf+mus pairs that aren't exact pairs, since EA's games can load any combo */
static STREAMFILE* open_mapfile_pair(STREAMFILE* sf, int track /*, int num_tracks*/) {
    static const char *const mapfile_pairs[][2] = {
        /* standard cases, replace map part with mus part (from the end to preserve prefixes) */
        {"MUS_CTRL.MPF",    "MUS_STR.MUS"}, /* GoldenEye - Rogue Agent (PS2) */
        {"mus_ctrl.mpf",    "mus_str.mus"}, /* GoldenEye - Rogue Agent (others) */
        {"AKA_Mus.mpf",     "Track.mus"}, /* Boogie */
        {"SSX4FE.mpf",      "TrackFE.mus"}, /* SSX On Tour */
        {"SSX4Path.mpf",    "Track.mus"},
        {"SSX4.mpf",        "moments0.mus,main.mus,load_loop0.mus"}, /* SSX Blur */
        {"*.mpf",            "*_main.mus"}, /* 007: Everything or Nothing */
        /* EA loads pairs manually, so complex cases needs .txtm to map
         * NSF2:
         * - ZTRxxROK.MAP > ZTRxx.TRJ
         * - ZTRxxTEC.MAP > ZTRxx.TRM 
         * - ZZSHOW.MAP and ZZSHOW2.MAP > ZZSHOW.MUS 
         * NSF3:
         * - ZTRxxROK.MAP > ZZZTRxxA.TRJ 
         * - ZTRxxTEC.MAP > ZZZTRxxB.TRM 
         * - ZTR00R0A.MAP and ZTR00R0B.MAP > ZZZTR00A.TRJ
         * SSX 3:
         * - *.mpf > *.mus,xxloops0.mus
         */
    };
    STREAMFILE* sf_mus = NULL;
    char file_name[PATH_LIMIT];
    int pair_count = (sizeof(mapfile_pairs)/sizeof(mapfile_pairs[0]));
    int i, j;
    size_t file_len, map_len;

    /* try parsing TXTM if present */
    sf_mus = read_filemap_file(sf, track);
    if (sf_mus) return sf_mus;

    /* if loading the first track, try opening MUS with the same name first (most common scenario) */
    if (track == 0) {
        sf_mus = open_streamfile_by_ext(sf, "mus");
        if (sf_mus) return sf_mus;
    }

    get_streamfile_filename(sf, file_name, PATH_LIMIT);
    file_len = strlen(file_name);

    for (i = 0; i < pair_count; i++) {
        const char *map_name = mapfile_pairs[i][0];
        const char *mus_name = mapfile_pairs[i][1];
        char buf[PATH_LIMIT] = {0};
        char *pch;
        int use_mask = 0;
        map_len = strlen(map_name);

        /* replace map_name with expected mus_name */
        if (file_len < map_len)
            continue;

        if (map_name[0] == '*') {
            use_mask = 1;
            map_name++;
            map_len--;

            if (strcmp(file_name + (file_len - map_len), map_name) != 0)
                continue;
        } else {
            if (strcmp(file_name, map_name) != 0)
                continue;
        }

        strncpy(buf, mus_name, PATH_LIMIT - 1);
        pch = strtok(buf, ","); //TODO: not thread safe in std C
        for (j = 0; j < track && pch; j++) {
            pch = strtok(NULL, ",");
        }
        if (!pch) continue; /* invalid track */

        if (use_mask) {
            file_name[file_len - map_len] = '\0';
            strncat(file_name, pch + 1, PATH_LIMIT - 1);
        } else {
            strncpy(file_name, pch, PATH_LIMIT - 1);
        }

        sf_mus = open_streamfile_by_filename(sf, file_name);
        if (sf_mus) return sf_mus;

        get_streamfile_filename(sf, file_name, PATH_LIMIT); /* reset for next loop */
    }

    /* hack when when multiple maps point to the same mus, uses name before "+"
     * ex. ZZZTR00A.TRJ+ZTR00PGR.MAP or ZZZTR00A.TRJ+ZTR00R0A.MAP both point to ZZZTR00A.TRJ
     * [Need for Speed II (PS1), Need for Speed III (PS1)] */
    {
        char *mod_name = strchr(file_name, '+');
        if (mod_name)
        {
            mod_name[0] = '\0';
            sf_mus = open_streamfile_by_filename(sf, file_name);
            if (sf_mus) return sf_mus;
        }
    }

    vgm_logi("EA MPF: .mus file not found (find and put together)\n");
    return NULL;
}

/* EA MAP/MUS combo - used in older games for interactive music (for EA's PathFinder tool) */
/* seen in Need for Speed II, Need for Speed III: Hot Pursuit, SSX */
VGMSTREAM* init_vgmstream_ea_map_mus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_mus = NULL;
    uint32_t schl_offset;
    uint8_t version, num_sounds, num_events, num_sections;
    off_t section_offset;
    int target_stream = sf->stream_index;

    /* check extension */
    if (!check_extensions(sf, "map,lin,mpf"))
        goto fail;

    /* always big endian */
    if (!is_id32be(0x00, sf, "PFDx"))
        goto fail;

    version = read_u8(0x04, sf);
    if (version > 1) goto fail;

    sf_mus = open_mapfile_pair(sf, 0); //, 1
    if (!sf_mus) goto fail;

    /*
     * 0x04: version
     * 0x05: starting node
     * 0x06: number of nodes
     * 0x07: number of events
     * 0x08: three zeroes
     * 0x0b: number of sections
     * 0x0c: data start
     */
    num_sounds = read_u8(0x06, sf);
    num_events = read_u8(0x07, sf);
    num_sections = read_u8(0x0b, sf);
    section_offset = 0x0c;

    /* section 1: nodes, contains information about segment playback order */
    section_offset += num_sounds * 0x1c;

    /* section 2: events, specific to game and track */
    section_offset += num_events * num_sections;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* section 3: samples */
    schl_offset = read_u32be(section_offset + (target_stream - 1) * 0x04, sf);
    if (read_u32be(schl_offset, sf_mus) != EA_BLOCKID_HEADER)
        goto fail;

    vgmstream = parse_schl_block(sf_mus, schl_offset);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = num_sounds;
    get_streamfile_filename(sf_mus, vgmstream->stream_name, STREAM_NAME_SIZE);
    close_streamfile(sf_mus);
    return vgmstream;

fail:
    close_streamfile(sf_mus);
    return NULL;
}

/* EA MPF/MUS combo - used in 6th gen games for interactive music (for EA's PathFinder tool) */
VGMSTREAM* init_vgmstream_ea_mpf_mus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_mus = NULL;
    segmented_layout_data *data_s = NULL;
    uint32_t tracks_table, tracks_data, samples_table = 0, section_offset, entry_offset = 0, eof_offset = 0, sound_offset,
        off_mult = 0, track_start, track_end = 0, track_checksum = 0;
    uint16_t num_nodes, num_subbanks = 0;
    uint8_t version, sub_version, num_tracks, num_sections, num_events, num_routers, num_vars, subentry_num = 0;
    int i;
    int target_stream = sf->stream_index, total_streams, big_endian, is_ram = 0;
    uint32_t(*read_u32)(off_t, STREAMFILE *);
    uint16_t(*read_u16)(off_t, STREAMFILE *);

    /* check extension */
    if (!check_extensions(sf, "mpf"))
        goto fail;

    /* detect endianness */
    if (is_id32be(0x00, sf, "PFDx")) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
        big_endian = 1;
    } else if (is_id32le(0x00, sf, "PFDx")) {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
        big_endian = 0;
    } else {
        goto fail;
    }

    version = read_u8(0x04, sf);
    sub_version = read_u8(0x05, sf);

    if (version < 3 || version > 5) goto fail;
    if (version == 5 && sub_version > 3) goto fail;

    num_tracks = read_u8(0x0d, sf);
    num_sections = read_u8(0x0e, sf);
    num_events = read_u8(0x0f, sf);
    num_routers = read_u8(0x10, sf);
    num_vars = read_u8(0x11, sf);
    num_nodes = read_u16(0x12, sf);

    /* Some structs here use C bitfields which are different on LE and BE AND their
     * implementation is compiler dependent, fun times.
     * Earlier versions don't have section offsets so we have to go through all of them
     * to get to the samples table. */

    if (target_stream == 0) target_stream = 1;

    if (version == 3 && (sub_version == 1 || sub_version == 2))
        /* SSX Tricky, Sled Storm */  {
        section_offset = 0x24;
        entry_offset = read_u16(section_offset + (num_nodes - 1) * 0x02, sf) * 0x04;
        subentry_num = read_u8(entry_offset + 0x0b, sf);
        section_offset = entry_offset + 0x0c + subentry_num * 0x04;

        section_offset += align_size_to_block(num_events * num_tracks * num_sections, 0x04);
        section_offset += num_routers * 0x04;
        section_offset += num_vars * 0x04;

        tracks_table = read_u32(section_offset, sf) * 0x04;
        samples_table = tracks_table + num_tracks * 0x04;
        eof_offset = get_streamfile_size(sf);
        total_streams = (eof_offset - samples_table) / 0x08;
        off_mult = 0x04;

        track_start = total_streams;

        for (i = num_tracks - 1; i >= 0; i--) {
            track_end = track_start;
            track_start = read_u32(tracks_table + i * 0x04, sf) * 0x04;
            track_start = (track_start - samples_table) / 0x08;
            if (track_start <= target_stream - 1)
                break;
        }
    } else if (version == 3 && sub_version == 4)
        /* Harry Potter and the Chamber of Secrets, Shox */ {
        section_offset = 0x24;
        entry_offset = read_u16(section_offset + (num_nodes - 1) * 0x02, sf) * 0x04;
        if (big_endian) {
            subentry_num = (read_u32be(entry_offset + 0x04, sf) >> 19) & 0x1F;
        } else {
            subentry_num = (read_u32be(entry_offset + 0x04, sf) >> 16) & 0x1F;
        }
        section_offset = entry_offset + 0x0c + subentry_num * 0x04;

        section_offset += align_size_to_block(num_events * num_tracks * num_sections, 0x04);
        section_offset += num_routers * 0x04;
        section_offset += num_vars * 0x04;

        tracks_table = read_u32(section_offset, sf) * 0x04;
        samples_table = tracks_table + (num_tracks + 1) * 0x04;
        eof_offset = read_u32(tracks_table + num_tracks * 0x04, sf) * 0x04;
        total_streams = (eof_offset - samples_table) / 0x08;
        off_mult = 0x04;

        track_start = total_streams;

        for (i = num_tracks - 1; i >= 0; i--) {
            track_end = track_start;
            track_start = read_u32(tracks_table + i * 0x04, sf) * 0x04;
            track_start = (track_start - samples_table) / 0x08;
            if (track_start <= target_stream - 1)
                break;
        }
    } else if (version == 4) {
        /* Need for Speed: Underground 2, SSX 3, Harry Potter and the Prisoner of Azkaban */
        section_offset = 0x20;
        entry_offset = read_u16(section_offset + (num_nodes - 1) * 0x02, sf) * 0x04;
        if (big_endian) {
            subentry_num = (read_u32be(entry_offset + 0x04, sf) >> 15) & 0x0F;
        } else {
            subentry_num = (read_u32be(entry_offset + 0x04, sf) >> 20) & 0x0F;
        }
        section_offset = entry_offset + 0x10 + subentry_num * 0x04;

        entry_offset = read_u16(section_offset + (num_events - 1) * 0x02, sf) * 0x04;
        if (big_endian) {
            subentry_num = (read_u32be(entry_offset + 0x0c, sf) >> 10) & 0x3F;
        } else {
            subentry_num = (read_u32be(entry_offset + 0x0c, sf) >> 8) & 0x3F;
        }
        section_offset = entry_offset + 0x10 + subentry_num * 0x10;

        section_offset += num_routers * 0x04;
        section_offset = read_u32(section_offset, sf) * 0x04;

        tracks_table = section_offset;
        samples_table = tracks_table + (num_tracks + 1) * 0x04;
        eof_offset = read_u32(tracks_table + num_tracks * 0x04, sf) * 0x04;
        total_streams = (eof_offset - samples_table) / 0x08;
        off_mult = 0x80;

        track_start = total_streams;

        for (i = num_tracks - 1; i >= 0; i--) {
            track_end = track_start;
            track_start = read_u32(tracks_table + i * 0x04, sf) * 0x04;
            track_start = (track_start - samples_table) / 0x08;
            if (track_start <= target_stream - 1)
                break;
        }
    } else if (version == 5) {
        /* Need for Speed: Most Wanted, Need for Speed: Carbon, SSX on Tour */
        tracks_table = read_u32(0x2c, sf);
        tracks_data = read_u32(0x30, sf);
        samples_table = read_u32(0x34, sf);
        eof_offset = read_u32(0x38, sf);
        total_streams = (eof_offset - samples_table) / 0x08;
        off_mult = 0x80;

        /* check to distinguish it from SNR/SNS version (first streamed sample is always at 0x00 or 0x100) */
        if (read_u16(tracks_data + 0x04, sf) == 0 && read_u32(samples_table + 0x00, sf) > 0x02)
            goto fail;

        track_start = total_streams;

        for (i = num_tracks - 1; i >= 0; i--) {
            track_end = track_start;
            entry_offset = read_u32(tracks_table + i * 0x04, sf) * 0x04;
            track_start = read_u32(entry_offset + 0x00, sf);

            if (track_start == 0 && i != 0)
                continue; /* empty track */

            if (track_start <= target_stream - 1) {
                num_subbanks = read_u16(entry_offset + 0x04, sf);
                track_checksum = read_u32be(entry_offset + 0x08, sf);
                is_ram = (num_subbanks != 0);
                break;
            }
        }
    } else {
        goto fail;
    }

    if (target_stream < 0 || total_streams == 0 || target_stream > total_streams)
        goto fail;

    /* open MUS file that matches this track */
    sf_mus = open_mapfile_pair(sf, i); //, num_tracks
    if (!sf_mus)
        goto fail;

    if (version < 5) {
        is_ram = (read_u32be(0x00, sf_mus) == (big_endian ? EA_BNK_HEADER_BE : EA_BNK_HEADER_LE));
    }

    /* 0x00 - offset/BNK index, 0x04 - duration (in milliseconds) */
    sound_offset = read_u32(samples_table + (target_stream - 1) * 0x08 + 0x00, sf);

    if (is_ram) {
        /* for some reason, RAM segments are almost always split into multiple sounds (usually 4) */
        off_t bnk_offset = version < 5 ? 0x00 : 0x100;
        uint32_t bnk_sound_index = (sound_offset & 0x0000FFFF);
        uint32_t bnk_index = (sound_offset & 0xFFFF0000) >> 16;
        uint32_t next_entry;
        uint32_t bnk_total_sounds = read_u16(bnk_offset + 0x06, sf_mus);
        int bnk_segments;

        if (version == 5 && bnk_index != 0) {
            /* HACK: open proper .mus now since open_mapfile_pair doesn't let us adjust the name */
            char filename[PATH_LIMIT], basename[PATH_LIMIT], ext[32];
            int basename_len;
            STREAMFILE* sf_temp;

            get_streamfile_basename(sf_mus, basename, PATH_LIMIT);
            basename_len = strlen(basename);
            get_streamfile_ext(sf_mus, ext, sizeof(ext));

            /* strip off 0 at the end */
            basename[basename_len - 1] = '\0';

            /* append bank index to the name */
            snprintf(filename, PATH_LIMIT, "%s%u.%s", basename, bnk_index, ext);

            sf_temp = open_streamfile_by_filename(sf_mus, filename);
            if (!sf_temp) goto fail;
            bnk_total_sounds = read_u16(bnk_offset + 0x06, sf_temp);
            close_streamfile(sf_mus);
            sf_mus = sf_temp;
        }

        if (version == 5) {
            track_checksum = read_u32be(entry_offset + 0x14 + 0x10 * bnk_index, sf);
            if (track_checksum && read_u32be(0x00, sf_mus) != track_checksum)
                goto fail;
        }

        if (read_u32be(bnk_offset, sf_mus) != (big_endian ? EA_BNK_HEADER_BE : EA_BNK_HEADER_LE))
            goto fail;

        /* play until the next entry in MPF track or the end of BNK */
        if (target_stream < track_end) {
            next_entry = read_u32(samples_table + (target_stream - 0) * 0x08 + 0x00, sf);
            if (((next_entry & 0xFFFF0000) >> 16) == bnk_index) {
                bnk_segments = (next_entry & 0x0000FFFF) - bnk_sound_index;
            } else {
                bnk_segments = bnk_total_sounds - bnk_sound_index;
            }
        } else {
            bnk_segments = bnk_total_sounds - bnk_sound_index;
        }

        /* init layout */
        data_s = init_layout_segmented(bnk_segments);
        if (!data_s) goto fail;

        for (i = 0; i < bnk_segments; i++) {
            data_s->segments[i] = parse_bnk_header(sf_mus, bnk_offset, bnk_sound_index + i, 1);
            if (!data_s->segments[i]) goto fail;
        }

        /* setup segmented VGMSTREAMs */
        if (!setup_layout_segmented(data_s)) goto fail;
        vgmstream = allocate_segmented_vgmstream(data_s, 0, 0, 0);
    } else {
        if (version == 5 && track_checksum && read_u32be(0x00, sf_mus) != track_checksum)
            goto fail;

        sound_offset *= off_mult;
        if (read_u32be(sound_offset, sf_mus) != EA_BLOCKID_HEADER)
            goto fail;

        vgmstream = parse_schl_block(sf_mus, sound_offset);
    }

    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_streams;
    get_streamfile_filename(sf_mus, vgmstream->stream_name, STREAM_NAME_SIZE);
    close_streamfile(sf_mus);
    return vgmstream;

fail:
    close_streamfile(sf_mus);
    free_layout_segmented(data_s);

    return NULL;
}

/* EA SCHl with variable header - from EA games (roughly 1997~2010); generated by EA Canada's sx.exe/Sound eXchange */
static VGMSTREAM* parse_schl_block(STREAMFILE* sf, off_t offset) {
    off_t start_offset, header_offset;
    size_t header_size;
    uint32_t header_id;
    ea_header ea = { 0 };

    /* use higher bits to store target localized block in case of multilang video,
     * so only header sub-id will be read and other langs skipped */
    header_id = read_32bitBE(offset + 0x00, sf);
    if ((header_id & 0xFFFF0000) == EA_BLOCKID_LOC_HEADER) {
        ea.codec_config |= (header_id & 0xFFFF) << 16;
    }

    if (guess_endianness32bit(offset + 0x04, sf)) { /* size is always LE, except in early SS/MAC */
        header_size = read_32bitBE(offset + 0x04, sf);
        ea.codec_config |= 0x02;
    }
    else {
        header_size = read_32bitLE(offset + 0x04, sf);
    }

    header_offset = offset + 0x08;

    if (!parse_variable_header(sf, &ea, header_offset, header_size - 0x08, 0))
        goto fail;

    start_offset = offset + header_size; /* starts in "SCCl" (skipped in block layout) or very rarely "SCDl" and maybe movie blocks */

    /* rest is common */
    return init_vgmstream_ea_variable_header(sf, &ea, start_offset, 0);

fail:
    return NULL;
}

/* EA BNK with variable header - from EA games SFXs; also created by sx.exe */
static VGMSTREAM* parse_bnk_header(STREAMFILE* sf, off_t offset, int target_stream, int is_embedded) {
    uint32_t i;
    uint16_t num_sounds;
    off_t header_offset, start_offset, test_offset, table_offset, entry_offset;
    size_t header_size;
    ea_header ea = { 0 };
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
    int16_t(*read_16bit)(off_t, STREAMFILE*) = NULL;
    VGMSTREAM *vgmstream = NULL;
    int bnk_version;
    int real_bnk_sounds = 0;

    /* check header */
    /* BNK header endianness is platform-native */
    if (read_32bitBE(offset + 0x00, sf) == EA_BNK_HEADER_BE) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else if (read_32bitBE(offset + 0x00, sf) == EA_BNK_HEADER_LE) {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    } else {
        goto fail;
    }

    bnk_version = read_8bit(offset + 0x04, sf);
    num_sounds = read_16bit(offset + 0x06, sf);

    /* check multi-streams */
    switch (bnk_version) {
        case 0x02: /* early [Need For Speed II (PC/PS1), FIFA 98 (PC/PS1/SAT)] */
            table_offset = 0x0c;
            header_size = read_32bit(offset + 0x08, sf); /* full size */
            break;

        case 0x04: /* mid (last used in PSX banks) */
        case 0x05: /* late (generated by sx.exe ~v2+) */
            /* 0x08: header/file size, 0x0C: file size/null, 0x10: always null */
            table_offset = 0x14;
            header_size = get_streamfile_size(sf); /* unknown (header is variable and may have be garbage until data) */
            break;

        default:
            VGM_LOG("EA BNK: unknown version %x\n", bnk_version);
            goto fail;
    }

    header_offset = 0;

    if (is_embedded) {
        if (target_stream < 0 || target_stream >= num_sounds)
            goto fail;

        entry_offset = offset + table_offset + 0x04 * target_stream;
        header_offset = entry_offset + read_32bit(entry_offset, sf);
    } else {
        /* some of these are dummies with zero offset, skip them when opening standalone BNK */
        for (i = 0; i < num_sounds; i++) {
            entry_offset = offset + table_offset + 0x04 * i;
            test_offset = read_32bit(entry_offset, sf);

            if (test_offset != 0) {
                if (target_stream == real_bnk_sounds)
                    header_offset = entry_offset + test_offset;

                real_bnk_sounds++;
            }
        }
    }

    if (header_offset == 0) goto fail;

    if (!parse_variable_header(sf, &ea, header_offset, header_size - header_offset, bnk_version))
        goto fail;

    /* fix absolute offsets so it works in next funcs */
    if (offset) {
        for (i = 0; i < ea.channels; i++) {
            ea.offsets[i] += offset;
        }
    }

    start_offset = ea.offsets[0]; /* first channel, presumably needed for MPEG */

    /* rest is common */
    vgmstream = init_vgmstream_ea_variable_header(sf, &ea, start_offset, bnk_version);
    if (!vgmstream) goto fail;
    if (!is_embedded) {
        vgmstream->num_streams = real_bnk_sounds;
    }

    return vgmstream;

fail:
    return NULL;
}

/* inits VGMSTREAM from a EA header */
static VGMSTREAM* init_vgmstream_ea_variable_header(STREAMFILE* sf, ea_header* ea, off_t start_offset, int bnk_version) {
    VGMSTREAM* vgmstream = NULL;
    int i, ch;
    int is_bnk = bnk_version;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ea->channels, ea->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ea->sample_rate;
    vgmstream->num_samples = ea->num_samples;
    vgmstream->loop_start_sample = ea->loop_start;
    vgmstream->loop_end_sample = ea->loop_end;

    vgmstream->codec_endian = ea->big_endian;
    vgmstream->codec_config = ea->codec_config;

    vgmstream->meta_type = is_bnk ? meta_EA_BNK : meta_EA_SCHL;
    vgmstream->layout_type = is_bnk ? layout_none : layout_blocked_ea_schl;

    /* EA usually implements their codecs in all platforms (PS2/WII do EAXA/MT/EALAYER3) and
     * favors them over platform's natives (ex. EAXA vs VAG/DSP).
     * Unneeded codecs are removed over time (ex. LAYER3 when EALAYER3 was introduced). */
    switch (ea->codec2) {

        case EA_CODEC2_EAXA_INT:    /* EA-XA (stereo) */
            vgmstream->coding_type = coding_EA_XA;
            break;

        case EA_CODEC2_EAXA:        /* EA-XA (split mono) */
            if (!ea->use_pcm_blocks) {
                /* original version */
                vgmstream->coding_type = coding_EA_XA_int;
            } else {
                /* later revision with PCM blocks and slighty modified decoding */
                vgmstream->coding_type = coding_EA_XA_V2;
            }
            break;

        case EA_CODEC2_S8_INT:      /* PCM8 (interleaved) */
            vgmstream->coding_type = coding_PCM8_int;
            break;

        case EA_CODEC2_S16LE_INT:   /* PCM16LE (interleaved) */
        case EA_CODEC2_S16BE_INT:   /* PCM16BE (interleaved) */
            vgmstream->coding_type = coding_PCM16_int;
            break;

        case EA_CODEC2_S8:          /* PCM8 (split) */
            vgmstream->coding_type = coding_PCM8;
            break;

        case EA_CODEC2_S16LE:       /* PCM16LE (split) */
            vgmstream->coding_type = coding_PCM16LE;
            break;

        case EA_CODEC2_S16BE:       /* PCM16BE (split) */
            vgmstream->coding_type = coding_PCM16BE;
            break;

        case EA_CODEC2_VAG:         /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            break;

        case EA_CODEC2_XBOXADPCM:   /* XBOX IMA (split mono) */
            vgmstream->coding_type = coding_XBOX_IMA_int;
            break;

        case EA_CODEC2_GCADPCM:     /* DSP */
            vgmstream->coding_type = coding_NGC_DSP;

            /* get them coefs (start offsets are not necessarily ordered) */
            {
                int16_t (*read_16bit)(off_t,STREAMFILE*) = ea->big_endian ? read_16bitBE : read_16bitLE;

                for (ch=0; ch < ea->channels; ch++) {
                    for (i=0; i < 16; i++) { /* actual size 0x21, last byte unknown */
                        vgmstream->ch[ch].adpcm_coef[i] = read_16bit(ea->coefs[ch] + i*2, sf);
                    }
                }
            }
            break;

        case EA_CODEC2_N64:         /* VADPCM */
            vgmstream->coding_type = coding_VADPCM;

            for (ch = 0; ch < ea->channels; ch++) {
                int order   = read_u32be(ea->coefs[ch] + 0x00, sf);
                int entries = read_u32be(ea->coefs[ch] + 0x04, sf);
                vadpcm_read_coefs_be(vgmstream, sf, ea->coefs[ch] + 0x08, order, entries, ch);
            }
            break;

#ifdef VGM_USE_MPEG
        case EA_CODEC2_LAYER2:      /* MPEG Layer II, aka MP2 */
        case EA_CODEC2_LAYER3: {    /* MPEG Layer III, aka MP3 */
            mpeg_custom_config cfg = {0};
            off_t mpeg_start_offset = is_bnk ?
                    start_offset :
                    get_ea_stream_mpeg_start_offset(sf, start_offset, ea);
            if (!mpeg_start_offset) goto fail;

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom(sf, mpeg_start_offset, &vgmstream->coding_type, ea->channels, MPEG_EA, &cfg);
            if (!vgmstream->codec_data) goto fail;
            break;
        }

        case EA_CODEC2_EALAYER3: {  /* MP3 variant */
            mpeg_custom_config cfg = {0};
            off_t mpeg_start_offset = is_bnk ?
                    start_offset :
                    get_ea_stream_mpeg_start_offset(sf, start_offset, ea);
            if (!mpeg_start_offset) goto fail;

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom(sf, mpeg_start_offset, &vgmstream->coding_type, ea->channels, MPEG_EAL31, &cfg);
            if (!vgmstream->codec_data) goto fail;
            break;
        }
#endif

        case EA_CODEC2_MT10:        /* MicroTalk (10:1 compression) */
        case EA_CODEC2_MT5:         /* MicroTalk (5:1 compression) */
            /* make relative loops absolute for the decoder */
            if (ea->loop_flag) {
                for (i = 0; i < ea->channels; i++) {
                    ea->loops[i] += ea->offsets[0];
                }
            }

            vgmstream->coding_type = coding_EA_MT;
            vgmstream->codec_data = init_ea_mt_loops(ea->channels, ea->use_pcm_blocks, ea->loop_start, ea->loops);
            if (!vgmstream->codec_data) goto fail;
            break;

#ifdef VGM_USE_FFMPEG
      //case EA_CODEC2_ATRAC3: /* works but commented to catch games using it */
        case EA_CODEC2_ATRAC3PLUS: { /* ATRAC3plus [Medal of Honor Heroes 2 (PSP)] */
            /* data chunked in SCxx blocks, including RIFF header */
            if (!is_bnk) {
                STREAMFILE* temp_sf = NULL;

                /* remove blocks on reads to feed FFmpeg a clean .at3 */
                temp_sf = setup_schl_streamfile(sf, ea->codec2, ea->channels, start_offset, 0);
                if (!temp_sf) goto fail;

                start_offset = 0x00; /* must point to the custom streamfile's beginning */
                ea->stream_size = get_streamfile_size(temp_sf);

                vgmstream->codec_data = init_ffmpeg_atrac3_riff(temp_sf, start_offset, NULL);
                close_streamfile(temp_sf);
            }
            else {
                /* memory file without blocks */
                vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf, start_offset, NULL);
            }

            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            VGM_LOG("EA SCHl: unknown codec2 0x%02x for platform 0x%02x\n", ea->codec2, ea->platform);
            goto fail;
    }

    vgmstream->stream_size = ea->stream_size;

    /* open files; channel offsets are updated below (force multibuffer for bnk) */
    if (!vgmstream_open_stream_bf(vgmstream, sf, start_offset, 1))
        goto fail;


    if (is_bnk) {
        /* BNKs usually have absolute offsets for all channels ("full" interleave) except in some versions */
        if (!(ea->codec_config & 0x04)) {
            switch (vgmstream->coding_type) {
                case coding_EA_XA:
                    /* shared (stereo version) */
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0];
                    }
                    break;
                case coding_EA_XA_int: {
                    int interleave = ea->num_samples / 28 * 0x0f; /* full interleave */
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] * interleave*i;
                    }
                    break;
                }
                case coding_PCM8_int:
                case coding_PCM16_int: {
                    int interleave = ea->bps==8 ? 0x01 : 0x02;
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
                    }
                    break;
                }
                case coding_PCM8:
                case coding_PCM16LE:
                case coding_PCM16BE: {
                    int interleave = ea->num_samples * (ea->bps==8 ? 0x01 : 0x02); /* full interleave */
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
                    }
                    break;
                }
                case coding_PSX: {
                    int interleave = ea->num_samples / 28 * 0x10; /* full interleave */
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
                    }
                    break;
                }
                case coding_VADPCM: {
                    uint32_t interleave = ea->flag_value;
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
                    }
                    break;
                }
                case coding_EA_MT: {
                    uint32_t interleave = ea->flag_value;
                    for (i = 0; i < vgmstream->channels; i++) {
                        vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
                    }
                    break;
                }
                default:
                    VGM_LOG("EA SCHl: Unknown channel offsets for codec 0x%02x in version %d\n", ea->codec1, ea->version);
                    goto fail;
            }
        } else if (vgmstream->coding_type == coding_NGC_DSP && vgmstream->channels > 1 && ea->offsets[0] == ea->offsets[1]) {
            /* pcstream+gcadpcm with sx.exe v2, not in flag_value, probably a bug (even with this parts of the wave are off) */
            int interleave = (ea->num_samples / 14 * 8); /* full interleave */
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = ea->offsets[0] + interleave*i;
            }
        } else if (ea->platform == EA_PLATFORM_PS2 && (ea->flag_value & 0x100)) {
            /* weird 0x10 mini header when played on IOP (codec/loop start/loop end/samples) [SSX 3 (PS2)] */
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = ea->offsets[i] + 0x10;
            }
        } else {
            /* absolute */
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = ea->offsets[i];
            }
        }

        /* TODO: Figure out how to get stream size for BNK sounds */
    } else {
        update_ea_stream_size(sf, start_offset, vgmstream);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static uint32_t read_patch(STREAMFILE* sf, off_t* offset) {
    uint32_t result = 0;
    uint8_t byte_count = read_8bit(*offset, sf);
    (*offset)++;

    if (byte_count == 0xFF) { /* signals 32b size (ex. custom user data) */
        (*offset) += 4 + read_32bitBE(*offset, sf);
        return 0;
    }

    if (byte_count > 4) { /* uncommon (ex. coef patches) */
        (*offset) += byte_count;
        return 0;
    }

    for ( ; byte_count > 0; byte_count--) { /* count of 0 is also possible, means value 0 */
        result <<= 8;
        result += (uint8_t)read_8bit(*offset, sf);
        (*offset)++;
    }

    return result;
}

/* decodes EA's GSTR/PT header (mostly cross-referenced with sx.exe) */
static int parse_variable_header(STREAMFILE* sf, ea_header* ea, off_t begin_offset, int max_length, int bnk_version) {
    off_t offset = begin_offset;
    uint32_t platform_id;
    int is_header_end = 0;
    int is_bnk = bnk_version;

    /* null defaults as 0 can be valid */
    ea->version = EA_VERSION_NONE;
    ea->codec1 = EA_CODEC1_NONE;
    ea->codec2 = EA_CODEC2_NONE;

    /* get platform info */
    platform_id = read_u32be(offset, sf);
    if (platform_id != get_id32be("GSTR") && (platform_id & 0xFFFF0000) != get_id32be("PT\0\0")) {
        offset += 4; /* skip unknown field (related to blocks/size?) in "nbapsstream" (NBA2000 PS, FIFA2001 PS) */
        platform_id = read_u32be(offset, sf);
    }

    if (platform_id == get_id32be("GSTR")) { /* Generic STReam */
        ea->platform = EA_PLATFORM_GENERIC;
        offset += 4 + 4; /* GSTRs have an extra field (config?): ex. 0x01000000, 0x010000D8 BE */
    }
    else if ((platform_id & 0xFFFF0000) == get_id32be("PT\0\0")) { /* PlaTform */
        ea->platform = read_u16le(offset + 2,sf);
        offset += 4;
    }
    else {
        goto fail;
    }


    /* parse mini-chunks/tags (variable, ommited if default exists; some are removed in later versions of sx.exe) */
    while (!is_header_end && offset - begin_offset < max_length) {
        uint8_t patch_type = read_u8(offset,sf);
        offset++;

        //;{ off_t test = offset; VGM_LOG("EA SCHl: patch=%02x at %lx, value=%x\n", patch_type, offset-1, read_patch(sf, &test)); }
        switch(patch_type) {
            case 0x00: /* signals non-default block rate and maybe other stuff; or padding after 0xFF */
                if (!is_header_end)
                    read_patch(sf, &offset);
                break;

            case 0x03: /* unknown (0x3c, rare: Madden NFL 2001 PS1) */
            case 0x04: /* unknown (0x3c, rare: Madden NFL 2001 PS1) */
            case 0x05: /* unknown (usually 0x50 except Madden NFL 3DS: 0x3e800) */
            case 0x06: /* priority (0..100, always 0x65 for streams, others for BNKs; rarely ommited) */
            case 0x07: /* unknown (BNK only: 36|3A|40) */
            case 0x08: /* release envelope (BNK only) */
            case 0x09: /* related to playback envelope (BNK only) */
            case 0x0A: /* bend range (BNK only) */
            case 0x0B: /* bank channels (or, offsets[] size; defaults to 1 if not present, removed in sx.exe v3) */
            case 0x0C: /* pan offset (BNK only) */
            case 0x0D: /* random pan offset range (BNK only) */
            case 0x0E: /* volume (BNK only) */
            case 0x0F: /* random volume range (BNK only) */
            case 0x10: /* detune (BNK only) */
            case 0x11: /* random detune range (BNK only) */
            case 0x12: /* unknown, rare (BNK only) [Need for Speed III: Hot Pursuit (PS1)] */
            case 0x13: /* effect bus (0..127) */
            case 0x14: /* emdedded user data (free size/value) */
            case 0x15: /* unknown, rare (BNK only) [Need for Speed: High Stakes (PS1)] */
            case 0x19: /* related to playback envelope (BNK only) */
            case 0x1B: /* unknown (movie only?) */
            case 0x1C: /* initial envelope volume (BNK only) */
            case 0x1D: /* unknown, rare [NASCAR 06 (Xbox)] */
            case 0x1E: /* related to ch1? (BNK only) */
            case 0x1F:
            case 0x20:
            case 0x21: /* related to ch2? (BNK only) */
            case 0x22:
            case 0x23:
            case 0x24: /* master random detune range (BNK only) */
            case 0x25: /* unknown */
                read_patch(sf, &offset);
                break;

            case 0xFC: /* padding for alignment between patches */
            case 0xFD: /* info section start marker */
                break;

            case 0x83: /* codec1 defines, used early revisions */
                ea->codec1 = read_patch(sf, &offset);
                break;
            case 0xA0: /* codec2 defines */
                ea->codec2 = read_patch(sf, &offset);
                break;

            case 0x80: /* version, affecting some codecs */
                ea->version = read_patch(sf, &offset);
                break;
            case 0x81: /* bits per sample for codec1 PCM */
                ea->bps = read_patch(sf, &offset);
                break;

            case 0x82: /* channel count */
                ea->channels = read_patch(sf, &offset);
                break;
            case 0x84: /* sample rate */
                ea->sample_rate = read_patch(sf,&offset);
                break;

            case 0x85: /* sample count */
                ea->num_samples = read_patch(sf, &offset);
                break;
            case 0x86: /* loop start sample */
                ea->loop_start = read_patch(sf, &offset);
                break;
            case 0x87: /* loop end sample */
                ea->loop_end = read_patch(sf, &offset) + 1; /* sx.exe does +1 */
                break;

            /* channel offsets (BNK only), can be the equal for all channels or interleaved; not necessarily contiguous */
            case 0x88: /* absolute offset of ch1 (or ch1+ch2 for stereo EAXA) */
                ea->offsets[0] = read_patch(sf, &offset);
                break;
            case 0x89: /* absolute offset of ch2 */
                ea->offsets[1] = read_patch(sf, &offset);
                break;
            case 0x94: /* absolute offset of ch3 */
                ea->offsets[2] = read_patch(sf, &offset);
                break;
            case 0x95: /* absolute offset of ch4 */
                ea->offsets[3] = read_patch(sf, &offset);
                break;
            case 0xA2: /* absolute offset of ch5 */
                ea->offsets[4] = read_patch(sf, &offset);
                break;
            case 0xA3: /* absolute offset of ch6 */
                ea->offsets[5] = read_patch(sf, &offset);
                break;

            case 0x8F: /* DSP/N64BLK coefs ch1 */
                ea->coefs[0] = offset+1;
                read_patch(sf, &offset);
                break;
            case 0x90: /* DSP/N64BLK coefs ch2 */
                ea->coefs[1] = offset+1;
                read_patch(sf, &offset);
                break;
            case 0x91: /* DSP coefs ch3, and unknown in older versions */
                ea->coefs[2] = offset+1;
                read_patch(sf, &offset);
                break;
            case 0xAB: /* DSP coefs ch4 */
                ea->coefs[3] = offset+1;
                read_patch(sf, &offset);
                break;
            case 0xAC: /* DSP coefs ch5 */
                ea->coefs[4] = offset+1;
                read_patch(sf, &offset);
                break;
            case 0xAD: /* DSP coefs ch6 */
                ea->coefs[5] = offset+1;
                read_patch(sf, &offset);
                break;

            case 0x1A: /* EA-MT/EA-XA relative loop offset of ch1 */
                ea->loops[0] = read_patch(sf, &offset);
                break;
            case 0x26: /* EA-MT/EA-XA relative loop offset of ch2 */
                ea->loops[1] = read_patch(sf, &offset);
                break;
            case 0x27: /* EA-MT/EA-XA relative loop offset of ch3 */
                ea->loops[2] = read_patch(sf, &offset);
                break;
            case 0x28: /* EA-MT/EA-XA relative loop offset of ch4 */
                ea->loops[3] = read_patch(sf, &offset);
                break;
            case 0x29: /* EA-MT/EA-XA relative loop offset of ch5 */
                ea->loops[4] = read_patch(sf, &offset);
                break;
            case 0x2a: /* EA-MT/EA-XA relative loop offset of ch6 */
                ea->loops[5] = read_patch(sf, &offset);
                break;

            case 0x8C: /* flags (ex. play type = 01=static/02=dynamic | spatialize = 20=pan/etc) */
                       /* (ex. PS1 VAG=0, PS2 PCM/LAYER2=4, GC EAXA=4, 3DS DSP=512, Xbox EAXA=36, N64 BLK=05E800, N64 MT10=01588805E800) */
                /* in rare cases value is the interleave, will be ignored if > 32b */
                ea->flag_value = read_patch(sf, &offset);
                break;

            case 0x8A: /* long padding (always 0x00000000) */
            case 0x8B: /* also padding? [Need for Speed: Hot Pursuit 2 (PC)] */
            case 0x8D: /* unknown, rare [FIFA 07 (GC)] */
            case 0x8E:
            case 0x92: /* bytes per sample? */
            case 0x93: /* unknown (BNK only) [Need for Speed III: Hot Pursuit (PC)] */
            case 0x98: /* embedded time stretch 1 (long data for who-knows-what) */
            case 0x99: /* embedded time stretch 2 */
            case 0x9C: /* azimuth ch1 */
            case 0x9D: /* azimuth ch2 */
            case 0x9E: /* azimuth ch3 */
            case 0x9F: /* azimuth ch4 */
            case 0xA6: /* azimuth ch5 */
            case 0xA7: /* azimuth ch6 */
            case 0xA1: /* unknown and very rare, always 0x02 [FIFA 2001 (PS2)] */
                read_patch(sf, &offset);
                break;

            case 0xFF: /* header end (then 0-padded so it's 32b aligned) */
                is_header_end = 1;
                break;
            case 0xFE: /* info subsection start marker (rare [SSX3 (PS2)]) */
                is_header_end = 1;
                /* Signals that another info section starts, redefining codec/samples/offsets/etc
                 * (previous header values should be cleared first as not everything is overwritten).
                 * This subsection seems the same as a next or prev PT subsong, so it's ignored. */
                break;

            default:
                VGM_LOG("EA SCHl: unknown patch 0x%02x at %x\n", patch_type, (uint32_t)offset);
                goto fail;
        }
    }

    if (ea->channels > EA_MAX_CHANNELS)
        goto fail;


    /* Set defaults per platform, as the header ommits them when possible */

    ea->loop_flag = (ea->loop_end);

    /* affects blocks/codecs */
    if (ea->platform == EA_PLATFORM_N64
        || ea->platform == EA_PLATFORM_MAC
        || ea->platform == EA_PLATFORM_SAT
        || ea->platform == EA_PLATFORM_GC
        || ea->platform == EA_PLATFORM_X360
        || ea->platform == EA_PLATFORM_PS3
        || ea->platform == EA_PLATFORM_WII
        || ea->platform == EA_PLATFORM_GENERIC) {
        ea->big_endian = 1;
    }

    if (!ea->channels) {
        ea->channels = 1;
    }

    /* version mainly affects defaults and minor stuff, can come with all codecs */
    /* V0 is often just null but it's specified in some files (uncommon, with patch size 0x00) */
    if (ea->version == EA_VERSION_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_PC:        ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PSX:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_N64:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_MAC:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_SAT:       ea->version = EA_VERSION_V0; break;
            case EA_PLATFORM_PS2:       ea->version = EA_VERSION_V1; break;
            case EA_PLATFORM_GC:        ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_XBOX:      ea->version = EA_VERSION_V2; break;
            case EA_PLATFORM_X360:      ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_PSP:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_PS3:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_WII:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_3DS:       ea->version = EA_VERSION_V3; break;
            case EA_PLATFORM_GENERIC:   ea->version = EA_VERSION_V2; break;
            default:
                VGM_LOG("EA SCHl: unknown default version for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* codec1 defaults */
    if (ea->codec1 == EA_CODEC1_NONE && ea->version == EA_VERSION_V0) {
        switch(ea->platform) {
            case EA_PLATFORM_PC:        ea->codec1 = EA_CODEC1_PCM; break;
            case EA_PLATFORM_PSX:       ea->codec1 = EA_CODEC1_VAG; break;
            case EA_PLATFORM_N64:       ea->codec1 = EA_CODEC1_N64; break;
            case EA_PLATFORM_MAC:       ea->codec1 = EA_CODEC1_PCM; break;
            case EA_PLATFORM_SAT:       ea->codec1 = EA_CODEC1_PCM; break;
            default:
                VGM_LOG("EA SCHl: unknown default codec1 for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* codec1 to codec2 to simplify later parsing */
    if (ea->codec1 != EA_CODEC1_NONE && ea->codec2 == EA_CODEC2_NONE) {
        switch (ea->codec1) {
            case EA_CODEC1_PCM:
                if (ea->platform == EA_PLATFORM_PC)
                    ea->codec2 = ea->bps==8 ? EA_CODEC2_S8_INT : (ea->big_endian ? EA_CODEC2_S16BE_INT : EA_CODEC2_S16LE_INT);
                else
                    ea->codec2 = ea->bps==8 ? EA_CODEC2_S8 : (ea->big_endian ? EA_CODEC2_S16BE : EA_CODEC2_S16LE);
                break;
            case EA_CODEC1_N64:         ea->codec2 = EA_CODEC2_N64; break;
            case EA_CODEC1_VAG:         ea->codec2 = EA_CODEC2_VAG; break;
            case EA_CODEC1_EAXA:
                if (ea->platform == EA_PLATFORM_PC || ea->platform == EA_PLATFORM_MAC)
                    ea->codec2 = EA_CODEC2_EAXA_INT;
                else
                    ea->codec2 = EA_CODEC2_EAXA;
                break;
            case EA_CODEC1_MT10:        ea->codec2 = EA_CODEC2_MT10; break;
            default:
                VGM_LOG("EA SCHl: unknown codec1 0x%02x\n", ea->codec1);
                goto fail;
        }
    }

    /* codec2 defaults */
    if (ea->codec2 == EA_CODEC2_NONE) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PC:        ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSX:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_N64:       ea->codec2 = EA_CODEC2_N64; break;
            case EA_PLATFORM_MAC:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PS2:       ea->codec2 = EA_CODEC2_VAG; break;
            case EA_PLATFORM_GC:        ea->codec2 = EA_CODEC2_S16BE; break;
            case EA_PLATFORM_XBOX:      ea->codec2 = EA_CODEC2_S16LE; break;
            case EA_PLATFORM_X360:      ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PSP:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_PS3:       ea->codec2 = EA_CODEC2_EAXA; break;
            case EA_PLATFORM_WII:       ea->codec2 = EA_CODEC2_GCADPCM; break;
            case EA_PLATFORM_3DS:       ea->codec2 = EA_CODEC2_GCADPCM; break;
            default:
                VGM_LOG("EA SCHl: unknown default codec2 for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* somehow doesn't follow machine's sample rate or anything sensical */
    if (!ea->sample_rate) {
        switch(ea->platform) {
            case EA_PLATFORM_GENERIC:   ea->sample_rate = 48000; break;
            case EA_PLATFORM_PC:        ea->sample_rate = 22050; break;
            case EA_PLATFORM_PSX:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_N64:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_MAC:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_SAT:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_PS2:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_GC:        ea->sample_rate = 24000; break;
            case EA_PLATFORM_XBOX:      ea->sample_rate = 24000; break;
            case EA_PLATFORM_X360:      ea->sample_rate = 44100; break;
            case EA_PLATFORM_PSP:       ea->sample_rate = 22050; break;
            case EA_PLATFORM_PS3:       ea->sample_rate = 44100; break;
            case EA_PLATFORM_WII:       ea->sample_rate = 32000; break;
            case EA_PLATFORM_3DS:       ea->sample_rate = 32000; break;
            default:
                VGM_LOG("EA SCHl: unknown default sample rate for platform 0x%02x\n", ea->platform);
                goto fail;
        }
    }

    /* EA-XA and MicroTalk got updated revisions with PCM blocks in sx v2.30 */
    ea->use_pcm_blocks = (ea->version == EA_VERSION_V3 || (ea->version == EA_VERSION_V2 &&
        (ea->platform == EA_PLATFORM_PC ||
            ea->platform == EA_PLATFORM_MAC ||
            ea->platform == EA_PLATFORM_GENERIC)));

    /* some codecs have ADPCM hist at the start of every block in streams (but not BNKs) */
    if (!is_bnk) {
        if (ea->codec2 == EA_CODEC2_GCADPCM) {
            if (ea->platform == EA_PLATFORM_3DS)
                ea->codec_config |= 0x01;
        }
        else if (ea->codec2 == EA_CODEC2_EAXA) {
            /* EA-XA has ADPCM hist in the original version */
            if (!ea->use_pcm_blocks)
                ea->codec_config |= 0x01;
        }
    }

    if (ea->version > EA_VERSION_V0) {
        /* v0 needs channel offsets to be manually calculated
         * v1+ always has split channels and provides channel offsets */
        ea->codec_config |= 0x04;
    }

    return offset;

fail:
    return 0;
}

static void update_ea_stream_size(STREAMFILE* sf, off_t start_offset, VGMSTREAM* vgmstream) {
    uint32_t block_id;
    size_t stream_size = 0, file_size;

    /* formats with custom codecs */
    if (vgmstream->layout_type != layout_blocked_ea_schl)
        return;

    if (vgmstream->stream_size != 0)
        return;

    file_size = get_streamfile_size(sf);

    /* manually read totals */
    vgmstream->next_block_offset = start_offset;
    while (vgmstream->next_block_offset < file_size) {
        block_update_ea_schl(vgmstream->next_block_offset, vgmstream);
        if (vgmstream->current_block_samples < 0)
            break;

        block_id = read_32bitBE(vgmstream->current_block_offset + 0x00, sf);
        if (block_id == EA_BLOCKID_END) { /* banks should never contain movie "SHxx" */
            break;
        }

        if (vgmstream->current_block_samples > 0) {
            /* stream size is almost never provided in bank files so we have to calc it manually */
            stream_size += vgmstream->next_block_offset - vgmstream->ch[0].offset;
        }
    }

    /* reset once we're done */
    block_update_ea_schl(start_offset, vgmstream);
    vgmstream->stream_size = stream_size;
}

/* find data start offset inside the first SCDl; not very elegant but oh well */
static off_t get_ea_stream_mpeg_start_offset(STREAMFILE* sf, off_t start_offset, const ea_header* ea) {
    size_t file_size = get_streamfile_size(sf);
    off_t block_offset = start_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = ea->big_endian ? read_32bitBE : read_32bitLE;
    uint32_t header_lang = (ea->codec_config >> 16) & 0xFFFF;

    while (block_offset < file_size) {
        uint32_t block_id, block_size;
        off_t offset;

        block_id = read_32bitBE(block_offset+0x00,sf);

        block_size = read_32bitLE(block_offset+0x04,sf);
        if (block_size > 0x00F00000) /* size is always LE, except in early SAT/MAC */
            block_size = read_32bitBE(block_offset+0x04,sf);

        if (block_id == EA_BLOCKID_DATA || block_id == ((EA_BLOCKID_LOC_DATA | header_lang))) {
            /* "SCDl" or target "SDxx" multilang blocks */
            offset = read_32bit(block_offset+0x0c,sf); /* first value seems ok, second is something else in EALayer3 */
            return block_offset + 0x0c + ea->channels*0x04 + offset;
        }
        else if (block_id == 0x00000000) {
            goto fail; /* just in case */
        }
        else {
            block_offset += block_size; /* size includes header */
        }
    }

fail:
    return 0;
}
