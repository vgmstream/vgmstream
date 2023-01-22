#include <math.h>
#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ea_eaac_streamfile.h"

/* EAAudioCore (aka SND10) formats, EA's current audio middleware */

#define EAAC_VERSION_V0                 0x00 /* SNR/SNS */
#define EAAC_VERSION_V1                 0x01 /* SPS */

#define EAAC_CODEC_NONE                 0x00
#define EAAC_CODEC_RESERVED             0x01 /* EALAYER3 V1a? MP30/P6L0/P2B0/P2L0/P8S0/P8U0/PFN0? */
#define EAAC_CODEC_PCM16BE              0x02
#define EAAC_CODEC_EAXMA                0x03
#define EAAC_CODEC_XAS1                 0x04
#define EAAC_CODEC_EALAYER3_V1          0x05
#define EAAC_CODEC_EALAYER3_V2_PCM      0x06
#define EAAC_CODEC_EALAYER3_V2_SPIKE    0x07
#define EAAC_CODEC_GCADPCM              0x08
#define EAAC_CODEC_EASPEEX              0x09
#define EAAC_CODEC_EATRAX               0x0a
#define EAAC_CODEC_EAMP3                0x0b
#define EAAC_CODEC_EAOPUS               0x0c
#define EAAC_CODEC_EAATRAC9             0x0d
#define EAAC_CODEC_EAOPUSM              0x0e
#define EAAC_CODEC_EAOPUSMU             0x0f

#define EAAC_TYPE_RAM                   0x00
#define EAAC_TYPE_STREAM                0x01
#define EAAC_TYPE_GIGASAMPLE            0x02

#define EAAC_BLOCKID0_DATA              0x00
#define EAAC_BLOCKID0_END               0x80 /* maybe meant to be a bitflag? */

#define EAAC_BLOCKID1_HEADER            0x48 /* 'H' */
#define EAAC_BLOCKID1_DATA              0x44 /* 'D' */
#define EAAC_BLOCKID1_END               0x45 /* 'E' */

static VGMSTREAM* init_vgmstream_eaaudiocore_header(STREAMFILE* sf_head, STREAMFILE* sf_data, off_t header_offset, off_t start_offset, meta_t meta_type, int standalone);
static VGMSTREAM* parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t ast_offset);


/* .SNR+SNS - from EA latest games (~2005-2010), v0 header */
VGMSTREAM* init_vgmstream_ea_snr_sns(STREAMFILE* sf) {
    /* check extension, case insensitive */
    if (!check_extensions(sf,"snr"))
        goto fail;

    return init_vgmstream_eaaudiocore_header(sf, NULL, 0x00, 0x00, meta_EA_SNR_SNS, 1);

fail:
    return NULL;
}

/* .SPS - from EA latest games (~2010~present), v1 header */
VGMSTREAM* init_vgmstream_ea_sps(STREAMFILE* sf) {
    /* check extension, case insensitive */
    if (!check_extensions(sf,"sps"))
        goto fail;

    return init_vgmstream_eaaudiocore_header(sf, NULL, 0x00, 0x00, meta_EA_SPS, 1);

fail:
    return NULL;
}

/* .SNU - from EA Redwood Shores/Visceral games (Dead Space, Dante's Inferno, The Godfather 2), v0 header */
VGMSTREAM* init_vgmstream_ea_snu(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, header_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;

    /* check extension, case insensitive */
    if (!check_extensions(sf,"snu"))
        goto fail;

    /* EA SNU header (BE/LE depending on platform) */
    /* 0x00(1): related to sample rate? (03=48000)
     * 0x01(1): flags/count? (when set has extra block data before start_offset)
     * 0x02(1): always 0?
     * 0x03(1): channels? (usually matches but rarely may be 0)
     * 0x04(4): some size, maybe >>2 ~= number of frames
     * 0x08(4): start offset
     * 0x0c(4): some sub-offset? (0x20, found when @0x01 is set) */

    /* use start_offset as endianness flag */
    if (guess_endianness32bit(0x08,sf)) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    header_offset = 0x10; /* SNR header */
    start_offset = read_32bit(0x08,sf); /* SNS blocks */

    vgmstream = init_vgmstream_eaaudiocore_header(sf, sf, header_offset, start_offset, meta_EA_SNU, 0);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* EA ABK - ABK header seems to be same as in the old games but the sound table is different and it contains SNR/SNS sounds instead */
VGMSTREAM* init_vgmstream_ea_abk_eaac(STREAMFILE* sf) {
    int is_dupe, total_sounds = 0, target_stream = sf->stream_index;
    off_t bnk_offset, modules_table, module_data, player_offset, samples_table, entry_offset, ast_offset;
    off_t cfg_num_players_off, cfg_module_data_off, cfg_module_entry_size, cfg_samples_table_off;
    uint32_t i, j, k, num_sounds, num_sample_tables;
    uint16_t num_modules, bnk_index, bnk_target_index;
    uint8_t num_players;
    off_t sample_tables[0x400];
    VGMSTREAM* vgmstream;
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
    num_sample_tables = 0;
    bnk_target_index = 0xFFFF;
    ast_offset = 0;

    if (!bnk_offset || read_32bitBE(bnk_offset, sf) != 0x53313041) /* "S10A" */
        goto fail;

    /* set up some common values */
    if (modules_table == 0x5C) {
        /* the usual variant */
        cfg_num_players_off = 0x24;
        cfg_module_data_off = 0x2C;
        cfg_module_entry_size = 0x3C;
        cfg_samples_table_off = 0x04;
    } else if (modules_table == 0x78) {
        /* FIFA 08 has a bunch of extra zeroes all over the place, don't know what's up with that */
        cfg_num_players_off = 0x40;
        cfg_module_data_off = 0x54;
        cfg_module_entry_size = 0x68;
        cfg_samples_table_off = 0x0C;
    } else {
        goto fail;
    }

    for (i = 0; i < num_modules; i++) {
        num_players = read_8bit(modules_table + cfg_num_players_off, sf);
        module_data = read_32bit(modules_table + cfg_module_data_off, sf);
        if (num_players == 0xff) goto fail; /* EOF read */

        for (j = 0; j < num_players; j++) {
            player_offset = read_32bit(modules_table + cfg_module_entry_size + 0x04 * j, sf);
            samples_table = read_32bit(module_data + player_offset + cfg_samples_table_off, sf);

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
                /* 0x00: sound index */
                /* 0x02: priority */
                /* 0x03: azimuth */
                /* 0x08: streamed data offset */
                entry_offset = samples_table + 0x04 + 0x0C * k;
                bnk_index = read_16bit(entry_offset + 0x00, sf);

                /* some of these are dummies */
                if (bnk_index == 0xFFFF)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds) {
                    bnk_target_index = bnk_index;
                    ast_offset = read_32bit(entry_offset + 0x08, sf);
                }
            }
        }

        /* skip class controllers */
        num_players += read_8bit(modules_table + cfg_num_players_off + 0x03, sf);
        modules_table += cfg_module_entry_size + num_players * 0x04;
    }

    if (bnk_target_index == 0xFFFF || ast_offset == 0)
        goto fail;

    vgmstream = parse_s10a_header(sf, bnk_offset, bnk_target_index, ast_offset);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    return vgmstream;

fail:
    return NULL;
}

/* EA S10A header - seen inside new ABK files. Putting it here in case it's encountered stand-alone. */
static VGMSTREAM* parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t sns_offset) {
    uint32_t num_sounds;
    off_t snr_offset;
    STREAMFILE *astFile = NULL;
    VGMSTREAM* vgmstream;

    /* header is always big endian */
    /* 0x00: header magic */
    /* 0x04: version */
    /* 0x05: padding */
    /* 0x06: serial number */
    /* 0x08: number of files */
    /* 0x0C: offsets table */
    if (read_32bitBE(offset + 0x00, sf) != 0x53313041) /* "S10A" */
        goto fail;

    num_sounds = read_32bitBE(offset + 0x08, sf);
    if (num_sounds == 0 || target_index >= num_sounds)
        goto fail;

    snr_offset = offset + read_32bitBE(offset + 0x0C + 0x04 * target_index, sf);

    if (sns_offset == 0xFFFFFFFF) {
        /* RAM asset */
        //;VGM_LOG("EA S10A: RAM at snr=%lx", snr_offset);
        vgmstream = init_vgmstream_eaaudiocore_header(sf, NULL, snr_offset, 0x00, meta_EA_SNR_SNS, 0);
        if (!vgmstream)
            goto fail;
    } else {
        /* streamed asset */
        astFile = open_streamfile_by_ext(sf, "ast");
        if (!astFile) {
            vgm_logi("EA ABK: .ast file not found (find and put together)\n");
            goto fail;
        }

        if (read_32bitBE(0x00, astFile) != 0x53313053) /* "S10S" */
            goto fail;

        //;VGM_LOG("EA S10A: stream at snr=%lx, sns=%lx\n", snr_offset, sns_offset);
        vgmstream = init_vgmstream_eaaudiocore_header(sf, astFile, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
        if (!vgmstream)
            goto fail;

        close_streamfile(astFile);
    }

    return vgmstream;

fail:
    close_streamfile(astFile);
    return NULL;
}

/* EA SBR/SBS - used in older 7th gen games for storing SFX */
VGMSTREAM* init_vgmstream_ea_sbr(STREAMFILE* sf) {
    uint32_t num_sounds, sound_id, type_desc, num_items, item_type,
        table_offset, types_offset, entry_offset, items_offset, data_offset, snr_offset, sns_offset;
    uint32_t i;
    STREAMFILE *sf_sbs = NULL;
    VGMSTREAM* vgmstream = NULL;
    int target_stream = sf->stream_index;

    if (!check_extensions(sf, "sbr"))
        goto fail;

    if (read_u32be(0x00, sf) != 0x53424B52) /* "SBKR" */
        goto fail;

    /* SBR files are always big endian */
    num_sounds = read_u32be(0x1c, sf);
    table_offset = read_u32be(0x24, sf);
    types_offset = read_u32be(0x28, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + 0x0a * (target_stream - 1);
    sound_id = read_u32be(entry_offset + 0x00, sf);
    num_items = read_u16be(entry_offset + 0x04, sf);
    items_offset = read_u32be(entry_offset + 0x06, sf);

    snr_offset = 0;
    sns_offset = 0;

    for (i = 0; i < num_items; i++) {
        entry_offset = items_offset + 0x06 * i;
        item_type = read_u16be(entry_offset + 0x00, sf);
        data_offset = read_u32be(entry_offset + 0x02, sf);

        type_desc = read_u32be(types_offset + 0x06 * item_type, sf);

        switch (type_desc) {
            case 0x534E5231: /* "SNR1" */
                snr_offset = data_offset;
                break;
            case 0x534E5331: /* "SNS1" */
                sns_offset = read_u32be(data_offset, sf);
                break;
            default:
                break;
        }
    }

    if (snr_offset == 0 && sns_offset == 0)
        goto fail;

    if (sns_offset == 0) {
        /* RAM asset */
        meta_t meta_type = (read_u8(snr_offset, sf) == 0x48) ? meta_EA_SPS : meta_EA_SNR_SNS;
        vgmstream = init_vgmstream_eaaudiocore_header(sf, NULL, snr_offset, 0x00, meta_type, 0);
        if (!vgmstream) goto fail;
    } else {
        /* streamed asset */
        sf_sbs = open_streamfile_by_ext(sf, "sbs");
        if (!sf_sbs) goto fail;

        if (read_u32be(0x00, sf_sbs) != 0x53424B53) /* "SBKS" */
            goto fail;

        if (read_u8(sns_offset, sf_sbs) == 0x48) {
            /* SPS */
            vgmstream = init_vgmstream_eaaudiocore_header(sf_sbs, NULL, sns_offset, 0x00, meta_EA_SPS, 0);
            if (!vgmstream) goto fail;
        } else {
            /* SNR/SNS */
            if (snr_offset == 0)
                goto fail;

            vgmstream = init_vgmstream_eaaudiocore_header(sf, sf_sbs, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
            if (!vgmstream) goto fail;
        }
    }

    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%08x", sound_id);
    vgmstream->num_streams = num_sounds;
    close_streamfile(sf_sbs);
    return vgmstream;

fail:
    close_streamfile(sf_sbs);
    return NULL;
}

/* EA HDR/STH/DAT - seen in older 7th gen games, used for storing speech */
VGMSTREAM* init_vgmstream_ea_hdr_sth_dat(STREAMFILE* sf) {
    int target_stream = sf->stream_index;
    uint32_t snr_offset, sns_offset, block_size;
    uint16_t sth_offset, sth_offset2;
    uint8_t num_params, num_sounds, block_id;
    size_t dat_size;
    STREAMFILE *sf_dat = NULL, *sf_sth = NULL;
    VGMSTREAM* vgmstream;
    uint32_t(*read_u32)(off_t, STREAMFILE*);

    /* 0x00: ID */
    /* 0x02: number of parameters */
    /* 0x03: number of samples */
    /* 0x04: speaker ID (used for different police voices in NFS games) */
    /* 0x08: sample repeat (alt number of samples?) */
    /* 0x09: block size (always zero?) */
    /* 0x0a: number of blocks (related to size?) */
    /* 0x0c: number of sub-banks (always zero?) */
    /* 0x0e: padding */
    /* 0x10: table start */

    if (!check_extensions(sf, "hdr"))
        goto fail;

    if (read_u8(0x09, sf) != 0)
        goto fail;

    if (read_u32be(0x0c, sf) != 0)
        goto fail;

    /* first offset is always zero */
    if (read_u16be(0x10, sf) != 0)
        goto fail;

    sf_sth = open_streamfile_by_ext(sf, "sth");
    if (!sf_sth)
        goto fail;

    sf_dat = open_streamfile_by_ext(sf, "dat");
    if (!sf_dat)
        goto fail;

    /* STH always starts with the first offset of zero */
    sns_offset = read_u32be(0x00, sf_sth);
    if (sns_offset != 0)
        goto fail;

    /* check if DAT starts with a correct SNS block */
    block_id = read_u8(0x00, sf_dat);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    num_params = read_u8(0x02, sf) & 0x7F;
    num_sounds = read_u8(0x03, sf);

    if (read_u8(0x08, sf) > num_sounds)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    /* offsets in HDR are always big endian */
    sth_offset = read_u16be(0x10 + (0x02 + num_params) * (target_stream - 1), sf);

#if 0
    snr_offset = sth_offset + 0x04;
    sns_offset = read_u32(sth_offset + 0x00, sf_sth);
#else
    /* overly intricate way to detect byte endianness because of the simplicity of HDR format */
    dat_size = get_streamfile_size(sf_dat);
    snr_offset = 0;
    sns_offset = 0;

    if (num_sounds == 1) {
        /* always 0 */
        snr_offset = sth_offset + 0x04;
        sns_offset = 0x00;
    } else {
        /* find the first sound size and match it up with the second sound offset to detect endianness */
        while (1) {
            if (sns_offset >= dat_size)
                goto fail;

            block_id = read_u8(sns_offset, sf_dat);
            block_size = read_u32be(sns_offset, sf_dat) & 0x00FFFFFF;
            if (block_size == 0)
                goto fail;

            if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
                goto fail;

            sns_offset += block_size;

            if (block_id == EAAC_BLOCKID0_END)
                break;
        }

        sns_offset = align_size_to_block(sns_offset, 0x40);
        sth_offset2 = read_u16be(0x10 + (0x02 + num_params) * 1, sf);
        if (sns_offset == read_u32be(sth_offset2, sf_sth)) {
            read_u32 = read_u32be;
        } else if (sns_offset == read_u32le(sth_offset2, sf_sth)) {
            read_u32 = read_u32le;
        } else {
            goto fail;
        }

        snr_offset = sth_offset + 0x04;
        sns_offset = read_u32(sth_offset + 0x00, sf_sth);
    }
#endif

    block_id = read_u8(sns_offset, sf_dat);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    vgmstream = init_vgmstream_eaaudiocore_header(sf_sth, sf_dat, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
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
    close_streamfile(sf_sth);
    close_streamfile(sf_dat);
    return vgmstream;

fail:
    close_streamfile(sf_sth);
    close_streamfile(sf_dat);
    return NULL;
}

/* open map/mpf+mus pairs that aren't exact pairs, since EA's games can load any combo */
static STREAMFILE *open_mapfile_pair(STREAMFILE* sf, int track /*, int num_tracks*/) {
    static const char *const mapfile_pairs[][2] = {
        /* standard cases, replace map part with mus part (from the end to preserve prefixes) */
        {"game.mpf",        "Game_Stream.mus"}, /* Skate 1/2/3 */
        {"ipod.mpf",        "Ipod_Stream.mus"},
        {"world.mpf",       "World_Stream.mus"},
        {"FreSkate.mpf",    "track.mus,ram.mus"}, /* Skate It */
        {"nsf_sing.mpf",    "track_main.mus"}, /* Need for Speed: Nitro */
        {"nsf_wii.mpf",     "Track.mus"},
        {"ssx_fe.mpf",      "stream_1.mus,stream_2.mus"}, /* SSX 2012 */
        {"ssxdd.mpf",       "main_trk.mus,"
                            "trick_alaska0.mus,"
                            "trick_rockies0.mus,"
                            "trick_pata0.mus,"
                            "trick_ant0.mus,"
                            "trick_killi0.mus,"
                            "trick_cyb0.mus,"
                            "trick_hima0.mus,"
                            "trick_nz0.mus,"
                            "trick_alps0.mus,"
                            "trick_lhotse0.mus"}
    };
    STREAMFILE *sf_mus = NULL;
    char file_name[PATH_LIMIT];
    int pair_count = (sizeof(mapfile_pairs) / sizeof(mapfile_pairs[0]));
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
        char buf[PATH_LIMIT] = { 0 };
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

            if (strncmp(file_name + (file_len - map_len), map_name, map_len) != 0)
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
     * ex. ZZZTR00A.TRJ+ZTR00PGR.MAP or ZZZTR00A.TRJ+ZTR00R0A.MAP both point to ZZZTR00A.TRJ */
    {
        char *mod_name = strchr(file_name, '+');
        if (mod_name) {
            mod_name[0] = '\0';
            sf_mus = open_streamfile_by_filename(sf, file_name);
            if (sf_mus) return sf_mus;
        }
    }

    vgm_logi("EA MPF: .mus file not found (find and put together)\n");
    return NULL;
}

/* EA MPF/MUS combo - used in older 7th gen games for storing interactive music */
VGMSTREAM* init_vgmstream_ea_mpf_mus_eaac(STREAMFILE* sf) {
    uint32_t num_tracks, track_start, track_checksum = 0, mus_sounds, mus_stream = 0, bnk_index = 0, bnk_sound_index = 0,
        tracks_table, samples_table, eof_offset, table_offset, entry_offset = 0, snr_offset, sns_offset;
    uint16_t num_subbanks, index, sub_index;
    uint8_t version, sub_version;
    STREAMFILE *sf_mus = NULL;
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data_s = NULL;
    int i;
    int target_stream = sf->stream_index, total_streams, is_ram = 0;
    uint32_t(*read_u32)(off_t, STREAMFILE *);
    uint16_t(*read_u16)(off_t, STREAMFILE *);

    /* check extension */
    if (!check_extensions(sf, "mpf"))
        goto fail;

    /* detect endianness */
    if (is_id32be(0x00, sf, "PFDx")) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else if (is_id32le(0x00, sf, "PFDx")) {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    } else {
        goto fail;
    }

    version = read_u8(0x04, sf);
    sub_version = read_u8(0x05, sf);
    if (version != 5 || sub_version < 2 || sub_version > 3) goto fail;

    num_tracks = read_u8(0x0d, sf);

    tracks_table = read_u32(0x2c, sf);
    samples_table = read_u32(0x34, sf);
    eof_offset = read_u32(0x38, sf);
    total_streams = (eof_offset - samples_table) / 0x08;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || total_streams == 0 || target_stream > total_streams)
        goto fail;

    for (i = num_tracks - 1; i >= 0; i--) {
        entry_offset = read_u32(tracks_table + i * 0x04, sf) * 0x04;
        track_start = read_u32(entry_offset + 0x00, sf);

        if (track_start == 0 && i != 0)
            continue; /* empty track */

        if (track_start <= target_stream - 1) {
            num_subbanks = read_u16(entry_offset + 0x04, sf);
            track_checksum = read_u32be(entry_offset + 0x08, sf);
            is_ram = (num_subbanks != 0);
            mus_stream = target_stream - 1 - track_start;
            break;
        }
    }

    /* open MUS file that matches this track */
    sf_mus = open_mapfile_pair(sf, i);//, num_tracks
    if (!sf_mus)
        goto fail;

    /* sample offsets table is still there but it just holds SNS offsets, we only need it for RAM sound indexes */
    /* 0x00 - offset/index, 0x04 - duration (in milliseconds) */
    sns_offset = read_u32(samples_table + (target_stream - 1) * 0x08 + 0x00, sf);

    if (is_ram) {
        bnk_sound_index = (sns_offset & 0x0000FFFF);
        bnk_index = (sns_offset & 0xFFFF0000) >> 16;

        if (bnk_index != 0) {
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
            close_streamfile(sf_mus);
            sf_mus = sf_temp;
        }

        track_checksum = read_u32be(entry_offset + 0x14 + bnk_index * 0x10, sf);
        if (track_checksum && read_u32be(0x00, sf_mus) != track_checksum)
            goto fail;
    } else {
        if (track_checksum && read_u32be(0x00, sf_mus) != track_checksum)
            goto fail;
    }

    /* MUS file has a header, however */
    if (sub_version == 2) {
        if (read_u32(0x04, sf_mus) != 0x00)
            goto fail;

        /*
         * 0x00: index
         * 0x02: sub-index
         * 0x04: SNR offset
         * 0x08: SNS offset (contains garbage for RAM sounds)
         */
        table_offset = 0x08;

        if (is_ram) {
            int ram_segments;

            /* find number of parts for this node */
            for (i = 0; ; i++) {
                entry_offset = table_offset + (bnk_sound_index + i) * 0x0c;
                index = read_u16(entry_offset + 0x00, sf_mus);
                sub_index = read_u16(entry_offset + 0x02, sf_mus);

                if (index == 0xffff) /* EOF check */
                    goto fail;
                
                entry_offset += 0x0c;
                if (read_u16(entry_offset + 0x00, sf_mus) != index ||
                    read_u16(entry_offset + 0x02, sf_mus) != sub_index + 1)
                    break;
            }

            ram_segments = i + 1;

            /* init layout */
            data_s = init_layout_segmented(ram_segments);
            if (!data_s) goto fail;

            for (i = 0; i < ram_segments; i++) {
                entry_offset = table_offset + (bnk_sound_index + i) * 0x0c;
                snr_offset = read_u32(entry_offset + 0x04, sf_mus);
                data_s->segments[i] = init_vgmstream_eaaudiocore_header(sf_mus, NULL,
                    snr_offset, 0,
                    meta_EA_SNR_SNS, 0);
                if (!data_s->segments[i]) goto fail;
            }

            /* setup segmented VGMSTREAMs */
            if (!setup_layout_segmented(data_s)) goto fail;
            vgmstream = allocate_segmented_vgmstream(data_s, 0, 0, 0);
        } else {
            entry_offset = table_offset + mus_stream * 0x0c;
            snr_offset = read_u32(entry_offset + 0x04, sf_mus);
            sns_offset = read_u32(entry_offset + 0x08, sf_mus);

            vgmstream = init_vgmstream_eaaudiocore_header(sf_mus, sf_mus,
                snr_offset, sns_offset,
                meta_EA_SNR_SNS, 0);
        }
    } else if (sub_version == 3) {
        /* number of samples is always little endian */
        mus_sounds = read_u32le(0x04, sf_mus);
        if (mus_stream >= mus_sounds)
            goto fail;

        if (is_ram) {
            /* not seen so far */
            VGM_LOG("Found RAM track in MPF v5.3.\n");
            goto fail;
        }

        /*
         * 0x00: checksum
         * 0x04: index
         * 0x06: sub-index
         * 0x08: SNR offset
         * 0x0c: SNS offset
         * 0x10: SNR size
         * 0x14: SNS size
         * 0x18: zero
         */
        table_offset = 0x28;
        entry_offset = table_offset + mus_stream * 0x1c;
        snr_offset = read_u32(entry_offset + 0x08, sf_mus) * 0x10;
        sns_offset = read_u32(entry_offset + 0x0c, sf_mus) * 0x80;

        vgmstream = init_vgmstream_eaaudiocore_header(sf_mus, sf_mus,
            snr_offset, sns_offset,
            meta_EA_SNR_SNS, 0);
    } else {
        goto fail;
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

/* EA TMX - used for engine sounds in NFS games (2007-2011) */
VGMSTREAM* init_vgmstream_ea_tmx(STREAMFILE* sf) {
    uint32_t num_sounds, sound_type, table_offset, data_offset, entry_offset, sound_offset;
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int target_stream = sf->stream_index;
    uint32_t(*read_u32)(off_t, STREAMFILE *);

    if (!check_extensions(sf, "tmx"))
        goto fail;

    if (read_u32be(0x0c, sf) == 0x30303031) { /* "0001" */
        read_u32 = read_u32be;
    } else if (read_u32le(0x0c, sf) == 0x30303031) { /* "1000" */
        read_u32 = read_u32le;
    } else {
        goto fail;
    }

    num_sounds = read_u32(0x20, sf);
    table_offset = read_u32(0x58, sf);
    data_offset = read_u32(0x5c, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + (target_stream - 1) * 0x24;
    sound_type = read_u32(entry_offset + 0x00, sf);
    sound_offset = read_u32(entry_offset + 0x08, sf) + data_offset;

    switch (sound_type) {
        case 0x47494E20: /* "GIN " */
            temp_sf = setup_subfile_streamfile(sf, sound_offset, get_streamfile_size(sf) - sound_offset, "gin");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_gin(temp_sf);
            if (!vgmstream) goto fail;
            close_streamfile(temp_sf);
            break;
        case 0x534E5220: /* "SNR " */
            vgmstream = init_vgmstream_eaaudiocore_header(sf, NULL, sound_offset, 0x00, meta_EA_SNR_SNS, 0);
            if (!vgmstream) goto fail;
            break;
        default:
            goto fail;
    }

    vgmstream->num_streams = num_sounds;
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    return NULL;
}

/* EA Harmony Sample Bank - used in 8th gen EA Sports games */
VGMSTREAM* init_vgmstream_ea_sbr_harmony(STREAMFILE* sf) {
    uint64_t base_offset, sound_offset, offset, prev_offset;
    uint32_t dset_id, dset_offset, num_values, num_fields, field_id,
        data_offset, table_offset, set_sounds, sound_table_offset;
    int16_t flag;
    uint16_t num_dsets;
    uint8_t set_type, offset_size;
    char sound_name[STREAM_NAME_SIZE];
    STREAMFILE *sf_sbs = NULL, *sf_data = NULL;
    VGMSTREAM* vgmstream = NULL;
    int target_stream = sf->stream_index, total_sounds, local_target, is_streamed = 0;
    int i, j;
    uint64_t(*read_u64)(off_t, STREAMFILE *);
    uint32_t(*read_u32)(off_t, STREAMFILE*);
    uint16_t(*read_u16)(off_t, STREAMFILE*);

    if (!check_extensions(sf, "sbr"))
        goto fail;

    /* check header */
    if (is_id32be(0x00, sf, "SBle")) {
        read_u64 = read_u64le;
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    } else if (is_id32be(0x00, sf, "SBbe")) {
        read_u64 = read_u64be;
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        goto fail;
    }

    num_dsets = read_u16(0x0a, sf);
    table_offset = read_u32(0x18, sf);
    data_offset = read_u32(0x20, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    total_sounds = 0;
    sound_offset = 0;

    /* the bank is split into DSET sections each of which references one or multiple sounds */
    /* each set can contain RAM sounds (stored in SBR in data section) or streamed sounds (stored separately in SBS file) */
    for (i = 0; i < num_dsets; i++) {
        dset_offset = read_u32(table_offset + 0x08 * i, sf);
        if (read_u32(dset_offset, sf) != 0x44534554) /* "DSET" */
            goto fail;

        dset_id = read_u32(dset_offset + 0x08, sf);
        num_values = read_u32(dset_offset + 0x38, sf);
        num_fields = read_u32(dset_offset + 0x3c, sf);
        local_target = target_stream - total_sounds - 1;
        dset_offset += 0x48;

        /* find RAM or OFF field */
        for (j = 0; j < num_fields; j++) {
            field_id = read_u32(dset_offset, sf);
            if (field_id == 0x2E52414D || /* ".RAM" */
                field_id == 0x2E4F4646) { /* ".OFF" */
                break;
            }

            dset_offset += 0x18;
        }

        if (j == num_fields)
            goto fail;

        /* different set types store offsets differently */
        set_type = read_u8(dset_offset + 0x05, sf);

        /* data sets often contain duplicate offets, need to filter them out however we can */
        /* offsets are stored in ascending order which makes things easier */
        if (set_type == 0x00) {
            set_sounds = 1;
            total_sounds += set_sounds;
            if (local_target < 0 || local_target > 0)
                continue;

            sound_offset = read_u64(dset_offset + 0x08, sf);
        } else if (set_type == 0x01) {
            flag = (int16_t)read_u16(dset_offset + 0x06, sf);
            base_offset = read_u64(dset_offset + 0x08, sf);

            set_sounds = num_values;
            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            sound_offset = base_offset + flag * local_target;
        } else if (set_type == 0x02) {
            flag = (read_u16(dset_offset + 0x06, sf) >> 0) & 0xFF;
            offset_size = (read_u16(dset_offset + 0x06, sf) >> 8) & 0xFF;
            base_offset = read_u64(dset_offset + 0x08, sf);
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            set_sounds = 0;
            prev_offset = UINT64_MAX;
            for (j = 0; j < num_values; j++) {
                if (offset_size == 0x01) {
                    offset = read_u8(sound_table_offset + 0x01 * j, sf);
                } else if (offset_size == 0x02) {
                    offset = read_u16(sound_table_offset + 0x02 * j, sf);
                } else if (offset_size == 0x04) {
                    offset = read_u32(sound_table_offset + 0x04 * j, sf);
                } else {
                    goto fail;
                }
                offset <<= flag;
                offset += base_offset;

                if (offset != prev_offset) {
                    if (set_sounds == local_target)
                        sound_offset = offset;
                    set_sounds++;
                }
                prev_offset = offset;
            }

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;
        } else if (set_type == 0x03) {
            offset_size = (read_u16(dset_offset + 0x06, sf) >> 8) & 0xFF;
            set_sounds = read_u64(dset_offset + 0x08, sf);
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            if (offset_size == 0x01) {
                sound_offset = read_u8(sound_table_offset + 0x01 * local_target, sf);
            } else if (offset_size == 0x02) {
                sound_offset = read_u16(sound_table_offset + 0x02 * local_target, sf);
            } else if (offset_size == 0x04) {
                sound_offset = read_u32(sound_table_offset + 0x04 * local_target, sf);
            } else {
                goto fail;
            }
        } else if (set_type == 0x04) {
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            set_sounds = 0;
            prev_offset = UINT64_MAX;
            for (j = 0; j < num_values; j++) {
                offset = read_u64(sound_table_offset + 0x08 * j, sf);

                if (sound_offset != prev_offset) {
                    if (set_sounds == local_target)
                        sound_offset = offset;
                    set_sounds++;
                }
                prev_offset = offset;
            }

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;
        } else {
            goto fail;
        }

        snprintf(sound_name, STREAM_NAME_SIZE, "DSET %08x/%04d", dset_id, local_target);

        if (field_id == 0x2E52414D) { /* ".RAM" */
            is_streamed = 0;
        } else if (field_id == 0x2E4F4646) { /* ".OFF" */
            is_streamed = 1;
        }
    }

    if (sound_offset == 0)
        goto fail;

    if (!is_streamed) {
        /* RAM asset */
        if (!is_id32be(data_offset, sf, "data") &&
            !is_id32be(data_offset, sf, "DATA"))
            goto fail;

        sf_data = sf;
        sound_offset += data_offset;
    } else {
        /* streamed asset */
        sf_sbs = open_streamfile_by_ext(sf, "sbs");
        if (!sf_sbs)
            goto fail;

        if (!is_id32be(0x00, sf_sbs, "data") &&
            !is_id32be(0x00, sf_sbs, "DATA"))
            goto fail;

        sf_data = sf_sbs;

        if (is_id32be(sound_offset, sf_data, "slot")) {
            /* skip "slot" section */
            sound_offset += 0x30;
        }
    }

    vgmstream = init_vgmstream_eaaudiocore_header(sf_data, NULL, sound_offset, 0x00, meta_EA_SPS, 0);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    strncpy(vgmstream->stream_name, sound_name, STREAM_NAME_SIZE);
    close_streamfile(sf_sbs);
    return vgmstream;

fail:
    close_streamfile(sf_sbs);
    return NULL;
}

/* ************************************************************************* */

typedef struct {
    int version;
    int codec;
    int channel_config;
    int sample_rate;
    int type;

    int streamed;
    int channels;

    int num_samples;
    int loop_start;
    int loop_end;
    int loop_flag;
    int prefetch_samples;

    off_t stream_offset;
    off_t loop_offset;
    off_t prefetch_offset;
} eaac_header;

static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE* sf_head, STREAMFILE* sf_data, eaac_header* eaac);
static layered_layout_data* build_layered_eaaudiocore(STREAMFILE* sf, eaac_header *eaac, off_t start_offset);
static STREAMFILE *setup_eaac_streamfile(eaac_header *ea, STREAMFILE* sf_head, STREAMFILE* sf_data);
static size_t calculate_eaac_size(STREAMFILE* sf, eaac_header *ea, uint32_t num_samples, off_t start_offset, int is_ram);

/* EA newest header from RwAudioCore (RenderWare?) / EAAudioCore library (still generated by sx.exe).
 * Audio "assets" come in separate RAM headers (.SNR/SPH) and raw blocked streams (.SNS/SPS),
 * or together in pseudoformats (.SNU, .SBR+.SBS banks, .AEMS, .MUS, etc).
 * Some .SNR include stream data, while .SPS have headers so .SPH is optional. */
static VGMSTREAM* init_vgmstream_eaaudiocore_header(STREAMFILE* sf_head, STREAMFILE* sf_data, off_t header_offset, off_t start_offset, meta_t meta_type, int standalone) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *temp_sf = NULL, *sf = NULL, *snsFile = NULL;
    uint32_t header1, header2, header_block_size = 0, header_size;
    uint8_t header_block_id;
    eaac_header eaac = {0};

    if (meta_type == meta_EA_SPS) {
        header_block_id = read_8bit(header_offset, sf_head);
        header_block_size = read_32bitBE(header_offset, sf_head) & 0x00FFFFFF;
        if (header_block_id != EAAC_BLOCKID1_HEADER)
            goto fail;

        header_offset += 0x04;
    }

    /* EA SNR/SPH header */
    header1 = (uint32_t)read_32bitBE(header_offset + 0x00, sf_head);
    header2 = (uint32_t)read_32bitBE(header_offset + 0x04, sf_head);
    eaac.version        = (header1 >> 28) & 0x0F; /* 4 bits */
    eaac.codec          = (header1 >> 24) & 0x0F; /* 4 bits */
    eaac.channel_config = (header1 >> 18) & 0x3F; /* 6 bits */
    eaac.sample_rate    = (header1 >>  0) & 0x03FFFF; /* 18 bits */
    eaac.type           = (header2 >> 30) & 0x03; /* 2 bits */
    eaac.loop_flag      = (header2 >> 29) & 0x01; /* 1 bit */
    eaac.num_samples    = (header2 >>  0) & 0x1FFFFFFF; /* 29 bits */
    /* rest is optional, depends on used flags and codec (handled below) */

    /* common channel configs are mono/stereo/quad/5.1/7.1 (from debug strings), while others are quite rare
     * [Battlefield 4 (X360)-EAXMA: 3/5/7ch, Army of Two: The Devil's Cartel (PS3)-EALayer3v2P: 11ch] */
    eaac.channels = eaac.channel_config + 1;
    /* EA 6ch channel mapping is FL FC FR BL BR LFE, but may use stereo layers for dynamic music
     * instead, so we can't re-map automatically (use TXTP) */

    /* V0: SNR+SNS, V1: SPH+SPS (no apparent differences, other than block flags) */
    if (eaac.version != EAAC_VERSION_V0 && eaac.version != EAAC_VERSION_V1) {
        VGM_LOG("EA EAAC: unknown version\n");
        goto fail;
    }

    /* accepted max (some Dead Space 2 (PC) do use 96000) */
    if (eaac.sample_rate > 200000) {
        VGM_LOG("EA EAAC: unknown sample rate\n");
        goto fail;
    }

    /* catch unknown values */
    if (eaac.type != EAAC_TYPE_RAM && eaac.type != EAAC_TYPE_STREAM && eaac.type != EAAC_TYPE_GIGASAMPLE) {
        VGM_LOG("EA EAAC: unknown type 0x%02x\n", eaac.type);
        goto fail;
    }

    if (eaac.version == EAAC_VERSION_V1 && eaac.type == EAAC_TYPE_GIGASAMPLE) {
        /* probably does not exist */
        VGM_LOG("EA EAAC: bad stream type for version %x\n", eaac.version);
        goto fail;
    }

    /* Non-streamed sounds are stored as a single block (may not set block end flags) */
    eaac.streamed = (eaac.type != EAAC_TYPE_RAM);

    /* get loops (fairly involved due to the multiple layouts and mutant streamfiles)
     * full loops aren't too uncommon [Dead Space (PC) stream sfx/ambiance, FIFA 08 (PS3) RAM sfx],
     * while actual looping is very rare [Need for Speed: World (PC)-EAL3, The Simpsons Game (X360)-EAXMA] */

    /* get optional header values */
    header_size = 0x08;
    if (eaac.loop_flag) {
        header_size += 0x04;
        eaac.loop_start = read_32bitBE(header_offset + 0x08, sf_head);
        eaac.loop_end = eaac.num_samples;

        /* TODO: need more cases to test how layout/streamfiles react */
        if (eaac.loop_start > 0 && !(
            eaac.codec == EAAC_CODEC_EALAYER3_V1 ||
            eaac.codec == EAAC_CODEC_EALAYER3_V2_PCM ||
            eaac.codec == EAAC_CODEC_EALAYER3_V2_SPIKE ||
            eaac.codec == EAAC_CODEC_EAXMA ||
            eaac.codec == EAAC_CODEC_XAS1 ||
            eaac.codec == EAAC_CODEC_EATRAX)) {
            VGM_LOG("EA EAAC: unknown actual looping %i for codec %x\n", eaac.loop_start, eaac.codec);
            goto fail;
        }
    }

    switch (eaac.type) {
        case EAAC_TYPE_RAM:
            break;
        case EAAC_TYPE_STREAM:
            if (eaac.loop_flag) {
                header_size += 0x04;
                eaac.loop_offset = read_32bitBE(header_offset + 0x0c, sf_head);
            }
            break;
        case EAAC_TYPE_GIGASAMPLE: /* rarely seen [Def Jam Icon (X360)] */
            header_size += 0x04;
            eaac.prefetch_samples = read_32bitBE(header_offset + (eaac.loop_flag ? 0x0c : 0x08), sf_head);

            if (eaac.loop_flag && eaac.loop_start >= eaac.prefetch_samples) {
                header_size += 0x04;
                eaac.loop_offset = read_32bitBE(header_offset + 0x10, sf_head);
            }
            break;
        default:
            goto fail;
    }

    /* get data offsets */
    if (eaac.version == EAAC_VERSION_V0) {
        switch (eaac.type) {
            case EAAC_TYPE_RAM:
                eaac.stream_offset = header_offset + header_size;
                break;
            case EAAC_TYPE_STREAM:
                eaac.stream_offset = start_offset;
                break;
            case EAAC_TYPE_GIGASAMPLE:
                eaac.prefetch_offset = header_offset + header_size;
                eaac.stream_offset = start_offset;
                break;
            default:
                goto fail;
        }
    } else {
        eaac.stream_offset = header_offset - 0x04 + header_block_size;
    }

    /* correct loop offsets */
    if (eaac.loop_flag) {
        if (eaac.streamed) {
            /* SNR+SNS are separate so offsets are relative to the data start
             * (first .SNS block, or extra data before the .SNS block in case of .SNU)
             * SPS have headers+data together so offsets are relative to the file start [ex. FIFA 18 (PC)] */
            if (eaac.version == EAAC_VERSION_V0) {
                if (eaac.prefetch_samples != 0) {
                    if (eaac.loop_start == 0) {
                        /* loop from the beginning */
                        eaac.loop_offset = 0x00;
                    } else if (eaac.loop_start < eaac.prefetch_samples) {
                        /* loop from the second RAM block */
                        eaac.loop_offset = read_32bitBE(eaac.prefetch_offset, sf_head) & 0x00FFFFFF;
                    } else {
                        /* loop from offset within SNS */
                        eaac.loop_offset += read_32bitBE(eaac.prefetch_offset, sf_head) & 0x00FFFFFF;
                    }
                }
            } else {
                eaac.loop_offset -= header_block_size;
            }
        } else if (eaac.loop_start > 0) {
            /* RAM assets have two blocks in case of actual loops */
            /* find the second block by getting the first block size */
            eaac.loop_offset = read_32bitBE(eaac.stream_offset, sf_head) & 0x00FFFFFF;
        } else {
            /* RAM assets have only one block in case of full loops */
            eaac.loop_offset = 0x00; /* implicit */
        }
    }

    if (eaac.version == EAAC_VERSION_V0 && eaac.streamed) {
        /* open SNS file if needed */
        if (standalone) {
            snsFile = open_streamfile_by_ext(sf_head, "sns"); //todo clean
            sf_data = snsFile;
        }
        if (!sf_data) goto fail;
    }

    /* build streamfile with audio data */
    sf = setup_eaac_streamfile(&eaac, sf_head, sf_data);
    if (!sf) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(eaac.channels, eaac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = eaac.sample_rate;
    vgmstream->num_samples = eaac.num_samples;
    vgmstream->loop_start_sample = eaac.loop_start;
    vgmstream->loop_end_sample = eaac.loop_end;
    vgmstream->meta_type = meta_type;
    vgmstream->stream_size = get_streamfile_size(sf);

    /* EA decoder list and known internal FourCCs */
    switch(eaac.codec) {

        case EAAC_CODEC_PCM16BE: /* "P6B0": PCM16BE [NBA Jam (Wii)] */
            vgmstream->coding_type = coding_PCM16_int;
            vgmstream->codec_endian = 1;
            vgmstream->layout_type = layout_blocked_ea_sns;
            break;

#ifdef VGM_USE_FFMPEG
        case EAAC_CODEC_EAXMA: { /* "EXm0": EA-XMA [Dante's Inferno (X360)] */

            /* special (if hacky) loop handling, see comments */
            if (eaac.loop_start > 0) {
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, sf, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            }
            else {
                vgmstream->layout_data = build_layered_eaaudiocore(sf, &eaac, 0x00);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_layered;
            }

            break;
        }
#endif

        case EAAC_CODEC_XAS1: /* "Xas1": EA-XAS v1 [Dead Space (PC/PS3)] */

            /* special (if hacky) loop handling, see comments */
            if (eaac.loop_start > 0) {
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, sf, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            } else {
                vgmstream->coding_type = coding_EA_XAS_V1;
                vgmstream->layout_type = layout_blocked_ea_sns;
            }

            break;

#ifdef VGM_USE_MPEG
        case EAAC_CODEC_EALAYER3_V1:         /* "EL31": EALayer3 v1 [Need for Speed: Hot Pursuit (PS3)] */
        case EAAC_CODEC_EALAYER3_V2_PCM:     /* "L32P": EALayer3 v2 "PCM" [Battlefield 1943 (PS3)] */
        case EAAC_CODEC_EALAYER3_V2_SPIKE: { /* "L32S": EALayer3 v2 "Spike" [Dante's Inferno (PS3)] */
            mpeg_custom_config cfg = {0};
            mpeg_custom_t type = (eaac.codec == 0x05 ? MPEG_EAL31b : (eaac.codec == 0x06) ? MPEG_EAL32P : MPEG_EAL32S);

            /* EALayer3 needs custom IO that removes blocks on reads to fix some edge cases in L32P
             * and to properly apply discard modes (see ealayer3 decoder)
             * (otherwise, and after removing discard code, it'd work with layout_blocked_ea_sns) */

            /* special (if hacky) loop handling, see comments */
            if (eaac.loop_start > 0) {
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, sf, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            }
            else {
                temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00, 0);
                if (!temp_sf) goto fail;

                vgmstream->codec_data = init_mpeg_custom(temp_sf, 0x00, &vgmstream->coding_type, vgmstream->channels, type, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }

            break;
        }
#endif

        case EAAC_CODEC_GCADPCM: /* "Gca0": DSP [Need for Speed: Nitro (Wii) sfx] */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_ea_sns;
            /* DSP coefs are read in the blocks */
            break;

#ifdef VGM_USE_SPEEX
        case EAAC_CODEC_EASPEEX: { /* "Esp0": EASpeex (libspeex variant, base versions vary: 1.0.5, 1.2beta3) [FIFA 14 (PS4), FIFA 2020 (Switch)] */
            /* EASpeex looks normal but simplify with custom IO to avoid worrying about blocks.
             * First block samples count frames' samples subtracting encoder delay. */

            vgmstream->codec_data = init_speex_ea(eaac.channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_SPEEX;
            vgmstream->layout_type = layout_none;

            temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00, 0);
            if (!temp_sf) goto fail;

            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case EAAC_CODEC_EATRAX: { /* EATrax (unknown FourCC) [Need for Speed: Most Wanted (Vita)] */
            atrac9_config cfg = {0};

            /* EATrax is "buffered" ATRAC9, uses custom IO since it's kind of complex to add to the decoder */

#if 0
            /* For looped EATRAX, since we are using a deblocker SF no need to make segmented looping, though it works [Madden NFL 13 Demo (Vita)]
             * An issue with segmented is that AT9 state is probably not reset between loops, which segmented can't simulate */
            if (eaac.loop_start > 0) {
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf_head, sf, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            }
#endif

            cfg.channels = eaac.channels;
            /* sub-header after normal header */
            cfg.config_data = read_u32be(header_offset + header_size + 0x00,sf_head);
            /* 0x04: data size without blocks, LE b/c why make sense (but don't use it in case of truncated files) */
            /* 0x08: 16b frame size (same as config data) */

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00, 0);
            if (!temp_sf) goto fail;

            break;
        }
#endif


#ifdef VGM_USE_MPEG
        case EAAC_CODEC_EAMP3: { /* "EM30": EA-MP3 [Need for Speed 2015 (PS4), FIFA 2021 (PC)] */
            mpeg_custom_config cfg = {0};

            temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00, 0);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_mpeg_custom(temp_sf, 0x00, &vgmstream->coding_type, vgmstream->channels, MPEG_EAMP3, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case EAAC_CODEC_EAOPUS: { /* "Eop0": EAOpus [FIFA 17 (PC), FIFA 19 (Switch)]*/
            vgmstream->layout_data = build_layered_eaaudiocore(sf, &eaac, 0x00);
            if (!vgmstream->layout_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_layered;
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case EAAC_CODEC_EAOPUSM: /* "MSO0": Multi-Stream Opus [FIFA 2021 (PC)] */
        case EAAC_CODEC_EAOPUSMU: { /* "MSU0": Multi-Stream Opus Uncoupled [FIFA 2022 (PC)] */
            off_t offset = 0x00; // eaac.stream_offset;
            off_t data_size = get_streamfile_size(sf);
            opus_config cfg = {0};

            cfg.channels = eaac.channels;
            {
                uint32_t block_size = read_u32be(offset + 0x00, sf) & 0x00FFFFFF;
                uint32_t curr_samples = read_u32be(offset + 0x04, sf);
                uint32_t next_samples = read_u32be(offset + block_size + 0x04, sf);

                cfg.skip = next_samples - curr_samples;
                /* maybe should check if next block exists, but files of single packet? */
            }

            /* find coupled OPUS streams (internal streams using 2ch) */
            if (eaac.codec == EAAC_CODEC_EAOPUSMU) {
                cfg.coupled_count = 0;
            }
            else {
                switch(eaac.channels) {
                  //case 8:  cfg.coupled_count = 3; break;   /* 2ch+2ch+2ch+1ch+1ch, 5 streams */
                  //case 6:  cfg.coupled_count = 2; break;   /* 2ch+2ch+1ch+1ch, 4 streams */
                    case 4:  cfg.coupled_count = 2; break;   /* 2ch+2ch, 2 streams */
                    case 2:  cfg.coupled_count = 1; break;   /* 2ch, 1 stream */
                    case 1:  cfg.coupled_count = 0; break;   /* 1ch, 1 stream [Madden 22 (PC)] */
                    default: goto fail;                      /* possibly: streams = Nch / 2, coupled = Nch % 2 */
                }
            }

            /* total number internal OPUS streams (should be >0) */
            cfg.stream_count = cfg.channels - cfg.coupled_count;

            /* We *don't* remove EA blocks b/c in Multi Opus 1 block = 1 Opus packet
             * Regular EAOPUS uses layers to fake multichannel, this is normal multichannel Opus.
             * This can be used for stereo too, so probably replaces EAOPUS. */
            //temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,0,0, 0x00, 0);
            //if (!temp_sf) goto fail;

            vgmstream->codec_data = init_ffmpeg_ea_opusm(sf, offset, data_size, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case EAAC_CODEC_EAATRAC9: /* "AT90" (possibly ATRAC9 with a saner layout than EATRAX) */
        default:
            VGM_LOG("EA EAAC: unknown codec 0x%02x\n", eaac.codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, temp_sf ? temp_sf : sf, 0x00))
        goto fail;

    close_streamfile(sf);
    close_streamfile(snsFile);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(sf);
    close_streamfile(snsFile);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static size_t calculate_eaac_size(STREAMFILE *sf, eaac_header *ea, uint32_t num_samples, off_t start_offset, int is_ram) {
    uint32_t block_size;
    uint8_t block_id;
    size_t stream_size, file_size;
    off_t block_offset;
    int looped;

    file_size = get_streamfile_size(sf);
    block_offset = start_offset;
    stream_size = 0;
    looped = 0;

    while (block_offset < file_size) {
        block_id = read_8bit(block_offset, sf);
        block_size = read_32bitBE(block_offset, sf) & 0x00FFFFFF;
        if (block_size == 0 || block_size == 0x00FFFFFF)
            goto fail;

        /* stop when we reach the end marker */
        if (ea->version == EAAC_VERSION_V0) {
            if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
                goto fail;
        } else {
            if (block_id == EAAC_BLOCKID1_END)
                break;

            if (block_id != EAAC_BLOCKID1_DATA)
                goto fail;
        }

        stream_size += block_size;
        block_offset += block_size;

        if (ea->version == EAAC_VERSION_V0) {
            if (is_ram) {
                /* RAM data only consists of one block (two for looped sounds) */
                if (ea->loop_start > 0 && ea->loop_start < num_samples && !looped) looped = 1;
                else break;
            } else if (block_id == EAAC_BLOCKID0_END) {
                if (ea->loop_offset > 0 && ea->loop_start >= ea->prefetch_samples && !looped) looped = 1;
                else break;
            }
        }
    }

    return stream_size;

fail:
    return 0;
}


static STREAMFILE *setup_eaac_streamfile(eaac_header *ea, STREAMFILE* sf_head, STREAMFILE* sf_data) {
    size_t data_size;
    STREAMFILE *new_sf = NULL;
    STREAMFILE *temp_sf = NULL;
    STREAMFILE *sf_segments[2] = { 0 };

    if (ea->version == EAAC_VERSION_V0) {
        switch (ea->type) {
            case EAAC_TYPE_RAM:
                /* both header and data in SNR */
                data_size = calculate_eaac_size(sf_head, ea, ea->num_samples, ea->stream_offset, 1);
                if (data_size == 0) goto fail;

                new_sf = open_wrap_streamfile(sf_head);
                if (!new_sf) goto fail;
                temp_sf = new_sf;

                new_sf = open_clamp_streamfile(temp_sf, ea->stream_offset, data_size);
                if (!new_sf) goto fail;
                temp_sf = new_sf;
                break;
            case EAAC_TYPE_STREAM:
                /* header in SNR, data in SNS */
                data_size = calculate_eaac_size(sf_data, ea, ea->num_samples, ea->stream_offset, 0);
                if (data_size == 0) goto fail;

                new_sf = open_wrap_streamfile(sf_data);
                if (!new_sf) goto fail;
                temp_sf = new_sf;

                new_sf = open_clamp_streamfile(temp_sf, ea->stream_offset, data_size);
                if (!new_sf) goto fail;
                temp_sf = new_sf;
                break;
            case EAAC_TYPE_GIGASAMPLE:
                /* header and prefetched data in SNR, rest of data in SNS */
                /* open prefetched data */
                data_size = calculate_eaac_size(sf_head, ea, ea->prefetch_samples, ea->prefetch_offset, 1);
                if (data_size == 0) goto fail;

                new_sf = open_wrap_streamfile(sf_head);
                if (!new_sf) goto fail;
                sf_segments[0] = new_sf;

                new_sf = open_clamp_streamfile(sf_segments[0], ea->prefetch_offset, data_size);
                if (!new_sf) goto fail;
                sf_segments[0] = new_sf;

                /* open main data */
                data_size = calculate_eaac_size(sf_data, ea, ea->num_samples - ea->prefetch_samples, ea->stream_offset, 0);
                if (data_size == 0) goto fail;

                new_sf = open_wrap_streamfile(sf_data);
                if (!new_sf) goto fail;
                sf_segments[1] = new_sf;

                new_sf = open_clamp_streamfile(sf_segments[1], ea->stream_offset, data_size);
                if (!new_sf) goto fail;
                sf_segments[1] = new_sf;

                new_sf = open_multifile_streamfile(sf_segments, 2);
                if (!new_sf) goto fail;
                temp_sf = new_sf;
                sf_segments[0] = NULL;
                sf_segments[1] = NULL;
                break;
            default:
                goto fail;
        }
    } else {
        data_size = calculate_eaac_size(sf_head, ea, ea->num_samples, ea->stream_offset, 0);
        if (data_size == 0) goto fail;

        new_sf = open_wrap_streamfile(sf_head);
        if (!new_sf) goto fail;
        temp_sf = new_sf;

        new_sf = open_clamp_streamfile(temp_sf, ea->stream_offset, data_size);
        if (!new_sf) goto fail;
        temp_sf = new_sf;
    }

    return temp_sf;

fail:
    close_streamfile(sf_segments[0]);
    close_streamfile(sf_segments[1]);
    close_streamfile(temp_sf);

    return NULL;
}

/* Actual looping uses 2 block sections, separated by a block end flag *and* padded.
 *
 * We use the segmented layout, since the eaac_streamfile doesn't handle padding,
 * and the segments seem fully separate (so even skipping would probably decode wrong). */
// todo reorganize code for more standard init
static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE *sf_head, STREAMFILE *sf_data, eaac_header *eaac) {
    segmented_layout_data *data = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t data_size = get_streamfile_size(sf_data);
    off_t offsets[2] = { 0x00, eaac->loop_offset };
    uint32_t sizes[2] = { eaac->loop_offset, data_size - eaac->loop_offset };
    off_t start_offset;
    int num_samples[2] = { eaac->loop_start, eaac->num_samples - eaac->loop_start};
    int segment_count = 2; /* intro/loop */
    int i;


    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    for (i = 0; i < segment_count; i++) {
        VGMSTREAM* vgmstream = allocate_vgmstream(eaac->channels, 0);

        if (!vgmstream) goto fail;
        data->segments[i] = vgmstream;

        data->segments[i]->sample_rate = eaac->sample_rate;
        data->segments[i]->num_samples = num_samples[i];
        //data->segments[i]->meta_type = eaac->meta_type; /* bleh */

        switch(eaac->codec) {
#ifdef VGM_USE_FFMPEG
            case EAAC_CODEC_EAXMA: {
                eaac_header temp_eaac = *eaac; /* equivalent to memcpy... I think */
                temp_eaac.loop_flag = 0;
                temp_eaac.num_samples = num_samples[i];

                start_offset = 0x00; /* must point to the custom streamfile's beginning */

                /* layers inside segments, how trippy */
                data->segments[i]->layout_data = build_layered_eaaudiocore(sf_data, &temp_eaac, offsets[i]);
                if (!data->segments[i]->layout_data) goto fail;
                data->segments[i]->coding_type = coding_FFmpeg;
                data->segments[i]->layout_type = layout_layered;
                break;
            }
#endif

            case EAAC_CODEC_XAS1: {
                start_offset = offsets[i];

                data->segments[i]->coding_type = coding_EA_XAS_V1;
                data->segments[i]->layout_type = layout_blocked_ea_sns;
                break;
            }

#ifdef VGM_USE_MPEG
            case EAAC_CODEC_EALAYER3_V1:
            case EAAC_CODEC_EALAYER3_V2_PCM:
            case EAAC_CODEC_EALAYER3_V2_SPIKE: {
                mpeg_custom_config cfg = {0};
                mpeg_custom_t type = (eaac->codec == 0x05 ? MPEG_EAL31b : (eaac->codec == 0x06) ? MPEG_EAL32P : MPEG_EAL32S);

                start_offset = 0x00; /* must point to the custom streamfile's beginning */

                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version,eaac->codec,eaac->streamed,0,0, offsets[i], sizes[i]);
                if (!temp_sf) goto fail;

                data->segments[i]->codec_data = init_mpeg_custom(temp_sf, 0x00, &data->segments[i]->coding_type, eaac->channels, type, &cfg);
                if (!data->segments[i]->codec_data) goto fail;
                data->segments[i]->layout_type = layout_none;
                break;
            }
#endif
#ifdef VGM_USE_ATRAC9
            case EAAC_CODEC_EATRAX: { /* EATrax (unknown FourCC) [Need for Speed: Most Wanted (Vita)] */
                atrac9_config cfg = {0};

                /* EATrax is "buffered" ATRAC9, uses custom IO since it's kind of complex to add to the decoder */

                cfg.channels = eaac->channels;
                /* sub-header after normal header */
                cfg.config_data = read_u32be(0x14 + 0x00, sf_head); //todo pass header offset
                /* 0x04: data size without blocks, LE b/c why make sense (but don't use it in case of truncated files) */
                /* 0x08: 16b frame size (same as config data) */

                vgmstream->codec_data = init_atrac9(&cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_ATRAC9;
                vgmstream->layout_type = layout_none;

                start_offset = 0x00; /* must point to the custom streamfile's beginning */

                //todo should make sizes
                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,0,0, offsets[i], sizes[i]);
                if (!temp_sf) goto fail;

                break;
            }
#endif
            default:
                goto fail;
        }

        if (!vgmstream_open_stream(data->segments[i], temp_sf == NULL ? sf_data : temp_sf, start_offset))
            goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_segmented(data))
        goto fail;
    return data;

fail:
    close_streamfile(temp_sf);
    free_layout_segmented(data);
    return NULL;
}

static layered_layout_data* build_layered_eaaudiocore(STREAMFILE *sf_data, eaac_header *eaac, off_t start_offset) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    int i, layers = (eaac->channels+1) / 2;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (1/2ch streams: 2ch+2ch..+1ch or 2ch+2ch..+2ch). */
    for (i = 0; i < layers; i++) {
        int layer_channels = (i+1 == layers && eaac->channels % 2 == 1) ? 1 : 2; /* last layer can be 1/2ch */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, eaac->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = eaac->sample_rate;
        data->layers[i]->num_samples = eaac->num_samples;
        data->layers[i]->loop_start_sample = eaac->loop_start;
        data->layers[i]->loop_end_sample = eaac->loop_end;

        switch(eaac->codec) {
#ifdef VGM_USE_FFMPEG
            /* EA-XMA uses completely separate 1/2ch streams, unlike standard XMA that interleaves 1/2ch
             * streams with a skip counter to reinterleave (so EA-XMA streams don't have skips set) */
            case EAAC_CODEC_EAXMA: {
                int block_size;
                size_t stream_size;
                int is_xma1;

                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,i,layers, start_offset, 0);
                if (!temp_sf) goto fail;

                stream_size = get_streamfile_size(temp_sf);
                block_size = 0x10000;

                /* EA adopted XMA2 when it appeared around 2006, but detection isn't so easy
                 * (SNS with XMA2 do exist). Decoder should work when playing XMA1 as XMA2, but
                 * the other way around can cause issues, so it's safer to just use XMA2. */
                is_xma1 = 0; //eaac->version == EAAC_VERSION_V0; /* approximate */
                if (is_xma1) {
                    data->layers[i]->codec_data = init_ffmpeg_xma1_raw(temp_sf, 0x00, stream_size, data->layers[i]->channels, data->layers[i]->sample_rate, 0);
                }
                else {
                    data->layers[i]->codec_data = init_ffmpeg_xma2_raw(temp_sf, 0x00, stream_size, data->layers[i]->num_samples, data->layers[i]->channels, data->layers[i]->sample_rate, block_size, 0);
                }
                if (!data->layers[i]->codec_data) goto fail;

                data->layers[i]->coding_type = coding_FFmpeg;
                data->layers[i]->layout_type = layout_none;
                data->layers[i]->stream_size = get_streamfile_size(temp_sf);

                xma_fix_raw_samples(data->layers[i], temp_sf, 0x00,stream_size, 0, 0,0); /* samples are ok? */
                break;
            }

            /* Opus can do multichannel just fine, but that wasn't weird enough for EA */
            case EAAC_CODEC_EAOPUS: {
                int skip;
                size_t data_size;

                /* We'll remove EA blocks and pass raw data to FFmpeg Opus decoder */
                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,i,layers, start_offset, 0);
                if (!temp_sf) goto fail;

                skip = ea_opus_get_encoder_delay(0x00, temp_sf);
                data_size = get_streamfile_size(temp_sf);

                data->layers[i]->codec_data = init_ffmpeg_ea_opus(temp_sf, 0x00,data_size, layer_channels, skip, eaac->sample_rate);
                if (!data->layers[i]->codec_data) goto fail;
                data->layers[i]->coding_type = coding_FFmpeg;
                data->layers[i]->layout_type = layout_none;
                break;
            }
#endif
            default:
                goto fail;
        }

        if (!vgmstream_open_stream(data->layers[i], temp_sf, 0x00)) {
            goto fail;
        }

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;
    return data;

fail:
    close_streamfile(temp_sf);
    free_layout_layered(data);
    return NULL;
}
