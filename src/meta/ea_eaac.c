#include <math.h>
#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ea_eaac_streamfile.h"

/* EAAudioCore formats, EA's current audio middleware */

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

#define EAAC_TYPE_RAM                   0x00
#define EAAC_TYPE_STREAM                0x01
#define EAAC_TYPE_GIGASAMPLE            0x02

#define EAAC_BLOCKID0_DATA              0x00
#define EAAC_BLOCKID0_END               0x80 /* maybe meant to be a bitflag? */

#define EAAC_BLOCKID1_HEADER            0x48 /* 'H' */
#define EAAC_BLOCKID1_DATA              0x44 /* 'D' */
#define EAAC_BLOCKID1_END               0x45 /* 'E' */

static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE* sf_head, STREAMFILE* sf_data, off_t header_offset, off_t start_offset, meta_t meta_type, int standalone);
static VGMSTREAM *parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t ast_offset);
VGMSTREAM * init_vgmstream_gin_header(STREAMFILE* sf, off_t offset);


/* .SNR+SNS - from EA latest games (~2005-2010), v0 header */
VGMSTREAM * init_vgmstream_ea_snr_sns(STREAMFILE* sf) {
    /* check extension, case insensitive */
    if (!check_extensions(sf,"snr"))
        goto fail;

    return init_vgmstream_eaaudiocore_header(sf, NULL, 0x00, 0x00, meta_EA_SNR_SNS, 1);

fail:
    return NULL;
}

/* .SPS - from EA latest games (~2010~present), v1 header */
VGMSTREAM * init_vgmstream_ea_sps(STREAMFILE* sf) {
    /* check extension, case insensitive */
    if (!check_extensions(sf,"sps"))
        goto fail;

    return init_vgmstream_eaaudiocore_header(sf, NULL, 0x00, 0x00, meta_EA_SPS, 1);

fail:
    return NULL;
}

/* .SNU - from EA Redwood Shores/Visceral games (Dead Space, Dante's Inferno, The Godfather 2), v0 header */
VGMSTREAM * init_vgmstream_ea_snu(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
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
VGMSTREAM * init_vgmstream_ea_abk_eaac(STREAMFILE* sf) {
    int is_dupe, total_sounds = 0, target_stream = sf->stream_index;
    off_t bnk_offset, header_table_offset, base_offset, unk_struct_offset, table_offset, snd_entry_offset, ast_offset;
    off_t num_entries_off, base_offset_off, entries_off, sound_table_offset_off;
    uint32_t i, j, k, num_sounds, total_sound_tables;
    uint16_t num_tables, bnk_index, bnk_target_index;
    uint8_t num_entries, extra_entries;
    off_t sound_table_offsets[0x2000];
    VGMSTREAM *vgmstream;
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

    num_tables = read_16bit(0x0A, sf);
    header_table_offset = read_32bit(0x1C, sf);
    bnk_offset = read_32bit(0x20, sf);
    total_sound_tables = 0;
    bnk_target_index = 0xFFFF;
    ast_offset = 0;

    if (!bnk_offset || read_32bitBE(bnk_offset, sf) != 0x53313041) /* "S10A" */
        goto fail;

    /* set up some common values */
    if (header_table_offset == 0x5C) {
        /* the usual variant */
        num_entries_off = 0x24;
        base_offset_off = 0x2C;
        entries_off = 0x3C;
        sound_table_offset_off = 0x04;
    } else if (header_table_offset == 0x78) {
        /* FIFA 08 has a bunch of extra zeroes all over the place, don't know what's up with that */
        num_entries_off = 0x40;
        base_offset_off = 0x54;
        entries_off = 0x68;
        sound_table_offset_off = 0x0C;
    } else {
        goto fail;
    }

    for (i = 0; i < num_tables; i++) {
        num_entries = read_8bit(header_table_offset + num_entries_off, sf);
        extra_entries = read_8bit(header_table_offset + num_entries_off + 0x03, sf);
        base_offset = read_32bit(header_table_offset + base_offset_off, sf);
        if (num_entries == 0xff) goto fail; /* EOF read */

        for (j = 0; j < num_entries; j++) {
            unk_struct_offset = read_32bit(header_table_offset + entries_off + 0x04 * j, sf);
            table_offset = read_32bit(base_offset + unk_struct_offset + sound_table_offset_off, sf);

            /* For some reason, there are duplicate entries pointing at the same sound tables */
            is_dupe = 0;
            for (k = 0; k < total_sound_tables; k++) {
                if (table_offset == sound_table_offsets[k]) {
                    is_dupe = 1;
                    break;
                }
            }

            if (is_dupe)
                continue;

            sound_table_offsets[total_sound_tables++] = table_offset;
            num_sounds = read_32bit(table_offset, sf);
            if (num_sounds == 0xffffffff) goto fail; /* EOF read */

            for (k = 0; k < num_sounds; k++) {
                /* 0x00: sound index */
                /* 0x02: ??? */
                /* 0x04: ??? */
                /* 0x08: streamed data offset */
                snd_entry_offset = table_offset + 0x04 + 0x0C * k;
                bnk_index = read_16bit(snd_entry_offset + 0x00, sf);

                /* some of these are dummies */
                if (bnk_index == 0xFFFF)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds) {
                    bnk_target_index = bnk_index;
                    ast_offset = read_32bit(snd_entry_offset + 0x08, sf);
                }
            }
        }

        header_table_offset += entries_off + num_entries * 0x04 + extra_entries * 0x04;
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
static VGMSTREAM * parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t sns_offset) {
    uint32_t num_sounds;
    off_t snr_offset;
    STREAMFILE *astFile = NULL;
    VGMSTREAM *vgmstream;

    /* header is always big endian */
    /* 0x00: header magic */
    /* 0x04: zero */
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
        if (!astFile)
            goto fail;

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
VGMSTREAM * init_vgmstream_ea_sbr(STREAMFILE* sf) {
    uint32_t i, num_sounds, type_desc;
    uint16_t num_metas, meta_type;
    off_t table_offset, types_offset, entry_offset, metas_offset, data_offset, snr_offset, sns_offset;
    STREAMFILE *sbsFile = NULL;
    VGMSTREAM *vgmstream = NULL;
    int target_stream = sf->stream_index;

    if (!check_extensions(sf, "sbr"))
        goto fail;

    if (read_32bitBE(0x00, sf) != 0x53424B52) /* "SBKR" */
        goto fail;

    /* SBR files are always big endian */
    num_sounds = read_32bitBE(0x1c, sf);
    table_offset = read_32bitBE(0x24, sf);
    types_offset = read_32bitBE(0x28, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + 0x0a * (target_stream - 1);
    num_metas = read_16bitBE(entry_offset + 0x04, sf);
    metas_offset = read_32bitBE(entry_offset + 0x06, sf);

    snr_offset = 0;
    sns_offset = 0;

    for (i = 0; i < num_metas; i++) {
        entry_offset = metas_offset + 0x06 * i;
        meta_type = read_16bitBE(entry_offset + 0x00, sf);
        data_offset = read_32bitBE(entry_offset + 0x02, sf);

        type_desc = read_32bitBE(types_offset + 0x06 * meta_type, sf);

        switch (type_desc) {
            case 0x534E5231: /* "SNR1" */
                snr_offset = data_offset;
                break;
            case 0x534E5331: /* "SNS1" */
                sns_offset = read_32bitBE(data_offset, sf);
                break;
            default:
                break;
        }
    }

    if (snr_offset == 0 && sns_offset == 0)
        goto fail;

    if (snr_offset == 0) {
        /* SPS file */
        sbsFile = open_streamfile_by_ext(sf, "sbs");
        if (!sbsFile)
            goto fail;

        if (read_32bitBE(0x00, sbsFile) != 0x53424B53) /* "SBKS" */
            goto fail;

        vgmstream = init_vgmstream_eaaudiocore_header(sbsFile, NULL, sns_offset, 0x00, meta_EA_SPS, 0);
        if (!vgmstream)
            goto fail;
    } else if (sns_offset == 0) {
        /* RAM asset */
        vgmstream = init_vgmstream_eaaudiocore_header(sf, NULL, snr_offset, 0x00, meta_EA_SNR_SNS, 0);
        if (!vgmstream)
            goto fail;
    } else {
        /* streamed asset */
        sbsFile = open_streamfile_by_ext(sf, "sbs");
        if (!sbsFile)
            goto fail;

        if (read_32bitBE(0x00, sbsFile) != 0x53424B53) /* "SBKS" */
            goto fail;

        vgmstream = init_vgmstream_eaaudiocore_header(sf, sbsFile, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
        if (!vgmstream)
            goto fail;
    }

    vgmstream->num_streams = num_sounds;
    close_streamfile(sbsFile);
    return vgmstream;

fail:
    close_streamfile(sbsFile);
    return NULL;
}

/* EA HDR/STH/DAT - seen in older 7th gen games, used for storing speech */
VGMSTREAM * init_vgmstream_ea_hdr_sth_dat(STREAMFILE* sf) {
    int target_stream = sf->stream_index;
    uint8_t userdata_size, total_sounds, block_id;
    off_t snr_offset, sns_offset, sth_offset, sth_offset2;
    size_t dat_size, block_size;
    STREAMFILE *datFile = NULL, *sthFile = NULL;
    VGMSTREAM *vgmstream;
    int32_t(*read_32bit)(off_t, STREAMFILE*);

    /* 0x00: ID */
    /* 0x02: userdata size */
    /* 0x03: number of files */
    /* 0x04: sub-ID (used for different police voices in NFS games) */
    /* 0x08: alt number of files? */
    /* 0x09: zero */
    /* 0x0A: related to size? */
    /* 0x0C: zero */
    /* 0x10: table start */

    if (!check_extensions(sf, "hdr"))
        goto fail;

    if (read_8bit(0x09, sf) != 0)
        goto fail;

    if (read_32bitBE(0x0c, sf) != 0)
        goto fail;

    /* first offset is always zero */
    if (read_16bitBE(0x10, sf) != 0)
        goto fail;

    sthFile = open_streamfile_by_ext(sf, "sth");
    if (!sthFile)
        goto fail;

    datFile = open_streamfile_by_ext(sf, "dat");
    if (!datFile)
        goto fail;

    /* STH always starts with the first offset of zero */
    sns_offset = read_32bitBE(0x00, sthFile);
    if (sns_offset != 0)
        goto fail;

    /* check if DAT starts with a correct SNS block */
    block_id = read_8bit(0x00, datFile);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    userdata_size = read_8bit(0x02, sf);
    total_sounds = read_8bit(0x03, sf);

    if (read_8bit(0x08, sf) > total_sounds)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || total_sounds == 0 || target_stream > total_sounds)
        goto fail;

    /* offsets in HDR are always big endian */
    sth_offset = (uint16_t)read_16bitBE(0x10 + (0x02 + userdata_size) * (target_stream - 1), sf);

#if 0
    snr_offset = sth_offset + 0x04;
    sns_offset = read_32bit(sth_offset + 0x00, sthFile);
#else
    /* we can't reliably detect byte endianness so we're going to find the sound the hacky way */
    dat_size = get_streamfile_size(datFile);
    snr_offset = 0;
    sns_offset = 0;

    if (total_sounds == 1) {
        /* always 0 */
        snr_offset = sth_offset + 0x04;
        sns_offset = 0x00;
    } else {
        /* find the first sound size and match it up with the second sound offset to detect endianness */
        while (1) {
            if (sns_offset >= dat_size)
                goto fail;

            block_id = read_8bit(sns_offset, datFile);
            block_size = read_32bitBE(sns_offset, datFile) & 0x00FFFFFF;
            if (block_size == 0)
                goto fail;

            if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
                goto fail;

            sns_offset += block_size;

            if (block_id == EAAC_BLOCKID0_END)
                break;
        }

        sth_offset2 = (uint16_t)read_16bitBE(0x10 + (0x02 + userdata_size) * 1, sf);
        if (sns_offset == read_32bitBE(sth_offset2, sthFile)) {
            read_32bit = read_32bitBE;
        } else if (sns_offset == read_32bitLE(sth_offset2, sthFile)) {
            read_32bit = read_32bitLE;
        } else {
            goto fail;
        }

        snr_offset = sth_offset + 0x04;
        sns_offset = read_32bit(sth_offset + 0x00, sthFile);
    }
#endif

    block_id = read_8bit(sns_offset, datFile);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    vgmstream = init_vgmstream_eaaudiocore_header(sthFile, datFile, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    close_streamfile(sthFile);
    close_streamfile(datFile);
    return vgmstream;

fail:
    close_streamfile(sthFile);
    close_streamfile(datFile);
    return NULL;
}

/* open map/mpf+mus pairs that aren't exact pairs, since EA's games can load any combo */
static STREAMFILE *open_mapfile_pair(STREAMFILE* sf, int track, int num_tracks) {
    static const char *const mapfile_pairs[][2] = {
        /* standard cases, replace map part with mus part (from the end to preserve prefixes) */
        {"game.mpf",        "Game_Stream.mus"}, /* Skate */
        {"ipod.mpf",        "Ipod_Stream.mus"},
        {"world.mpf",       "World_Stream.mus"},
        {"FreSkate.mpf",    "track.mus,ram.mus"}, /* Skate It */
        {"nsf_sing.mpf",    "track_main.mus"}, /* Need for Speed: Nitro */
        {"nsf_wii.mpf",     "Track.mus"}, /* Need for Speed: Nitro */
        {"ssx_fe.mpf",      "stream_1.mus,stream_2.mus"}, /* SSX 2012 */
        {"ssxdd.mpf",       "main_trk.mus," /* SSX 2012 */
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
    STREAMFILE *musFile = NULL;
    char file_name[PATH_LIMIT];
    int pair_count = (sizeof(mapfile_pairs) / sizeof(mapfile_pairs[0]));
    int i, j;
    size_t file_len, map_len;

    /* if loading the first track, try opening MUS with the same name first (most common scenario) */
    if (track == 0) {
        musFile = open_streamfile_by_ext(sf, "mus");
        if (musFile) return musFile;
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

        musFile = open_streamfile_by_filename(sf, file_name);
        if (musFile) return musFile;

        get_streamfile_filename(sf, file_name, PATH_LIMIT); /* reset for next loop */
    }

    /* hack when when multiple maps point to the same mus, uses name before "+"
     * ex. ZZZTR00A.TRJ+ZTR00PGR.MAP or ZZZTR00A.TRJ+ZTR00R0A.MAP both point to ZZZTR00A.TRJ */
    {
        char *mod_name = strchr(file_name, '+');
        if (mod_name) {
            mod_name[0] = '\0';
            musFile = open_streamfile_by_filename(sf, file_name);
            if (musFile) return musFile;
        }
    }

    VGM_LOG("No MPF/MUS pair specified for %s.\n", file_name);
    return NULL;
}

/* EA MPF/MUS combo - used in older 7th gen games for storing interactive music */
VGMSTREAM * init_vgmstream_ea_mpf_mus_eaac(STREAMFILE* sf) {
    uint32_t num_tracks, track_start, track_hash, mus_sounds, mus_stream = 0;
    uint8_t version, sub_version;
    off_t tracks_table, samples_table, eof_offset, table_offset, entry_offset, snr_offset, sns_offset;
    int32_t(*read_32bit)(off_t, STREAMFILE*);
    STREAMFILE *musFile = NULL;
    VGMSTREAM *vgmstream = NULL;
    int i;
    int target_stream = sf->stream_index, total_streams, is_ram = 0;

    /* check extension */
    if (!check_extensions(sf, "mpf"))
        goto fail;

    /* detect endianness */
    if (read_32bitBE(0x00, sf) == 0x50464478) { /* "PFDx" */
        read_32bit = read_32bitBE;
    } else if (read_32bitLE(0x00, sf) == 0x50464478) { /* "xDFP" */
        read_32bit = read_32bitLE;
    } else {
        goto fail;
    }

    version = read_8bit(0x04, sf);
    sub_version = read_8bit(0x05, sf);
    if (version != 5 || sub_version < 2 || sub_version > 3) goto fail;

    num_tracks = read_8bit(0x0d, sf);

    tracks_table = read_32bit(0x2c, sf);
    samples_table = read_32bit(0x34, sf);
    eof_offset = read_32bit(0x38, sf);
    total_streams = (eof_offset - samples_table) / 0x08;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || total_streams == 0 || target_stream > total_streams)
        goto fail;

    for (i = num_tracks - 1; i >= 0; i--) {
        entry_offset = read_32bit(tracks_table + i * 0x04, sf) * 0x04;
        track_start = read_32bit(entry_offset + 0x00, sf);

        if (track_start <= target_stream - 1) {
            track_hash = read_32bitBE(entry_offset + 0x08, sf);
            is_ram = (track_hash == 0xF1F1F1F1);

            /* checks to distinguish it from older versions */
            if (is_ram) {
                if (read_32bitBE(entry_offset + 0x0c, sf) != 0x00)
                    goto fail;

                track_hash = read_32bitBE(entry_offset + 0x14, sf);
                if (track_hash == 0xF1F1F1F1)
                    continue; /* empty track */
            } else {
                if (read_32bitBE(entry_offset + 0x0c, sf) == 0x00)
                    goto fail;
            }

            mus_stream = target_stream - 1 - track_start;
            break;
        }
    }

    /* open MUS file that matches this track */
    musFile = open_mapfile_pair(sf, i, num_tracks);
    if (!musFile)
        goto fail;

    if (read_32bitBE(0x00, musFile) != track_hash)
        goto fail;

    /* sample offsets table is still there but it just holds SNS offsets, it's of little use to us */
    /* MUS file has a header, however */
    if (sub_version == 2) {
        if (read_32bit(0x04, musFile) != 0x00)
            goto fail;

        /*
         * 0x00: flags? index?
         * 0x04: SNR offset
         * 0x08: SNS offset (contains garbage for RAM sounds)
         */
        table_offset = 0x08;
        entry_offset = table_offset + mus_stream * 0x0c;
        snr_offset = read_32bit(entry_offset + 0x04, musFile);
        sns_offset = read_32bit(entry_offset + 0x08, musFile);
    } else if (sub_version == 3) {
        /* number of files is always little endian */
        mus_sounds = read_32bitLE(0x04, musFile);
        if (mus_stream >= mus_sounds)
            goto fail;

        if (is_ram) {
            /* not seen so far */
            VGM_LOG("Found RAM SNR in MPF v5.3.\n");
            goto fail;
        }

        /*
         * 0x00: hash?
         * 0x04: index
         * 0x06: zero
         * 0x08: SNR offset
         * 0x0c: SNS offset
         * 0x10: SNR size
         * 0x14: SNS size
         * 0x18: zero
         */
        table_offset = 0x28;
        entry_offset = table_offset + mus_stream * 0x1c;
        snr_offset = read_32bit(entry_offset + 0x08, musFile) * 0x10;
        sns_offset = read_32bit(entry_offset + 0x0c, musFile) * 0x80;
    } else {
        goto fail;
    }

    vgmstream = init_vgmstream_eaaudiocore_header(musFile, musFile, snr_offset, sns_offset, meta_EA_SNR_SNS, 0);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_streams;
    get_streamfile_filename(musFile, vgmstream->stream_name, STREAM_NAME_SIZE);
    close_streamfile(musFile);
    return vgmstream;

fail:
    close_streamfile(musFile);
    return NULL;
}

/* EA TMX - used for engine sounds in NFS games (2007-present) */
VGMSTREAM * init_vgmstream_ea_tmx(STREAMFILE* sf) {
    uint32_t num_sounds, sound_type;
    off_t table_offset, data_offset, entry_offset, sound_offset;
    VGMSTREAM *vgmstream = NULL;
    int target_stream = sf->stream_index;

    if (!check_extensions(sf, "tmx"))
        goto fail;

    /* always little endian */
    if (read_32bitLE(0x0c, sf) != 0x30303031) /* "0001" */
        goto fail;

    num_sounds = read_32bitLE(0x20, sf);
    table_offset = read_32bitLE(0x58, sf);
    data_offset = read_32bitLE(0x5c, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + (target_stream - 1) * 0x24;
    sound_type = read_32bitLE(entry_offset + 0x00, sf);
    sound_offset = read_32bitLE(entry_offset + 0x08, sf) + data_offset;

    switch (sound_type) {
        case 0x47494E20: /* "GIN " */
            vgmstream = init_vgmstream_gin_header(sf, sound_offset);
            if (!vgmstream) goto fail;
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
    return NULL;
}

/* EA Harmony Sample Bank - used in 8th gen EA Sports games */
VGMSTREAM * init_vgmstream_ea_sbr_harmony(STREAMFILE *sf) {
    uint32_t num_dsets, set_sounds, chunk_id, data_offset, table_offset, dset_offset, base_offset, sound_table_offset, sound_offset;
    uint32_t i, j;
    uint8_t set_type, flag, offset_size;
    char sound_name[STREAM_NAME_SIZE];
    STREAMFILE *sbsFile = NULL, *sf_data = NULL;
    VGMSTREAM *vgmstream = NULL;
    int target_stream = sf->stream_index, total_sounds, local_target, is_streamed = 0;
    uint32_t(*read_u32)(off_t, STREAMFILE*);
    uint16_t(*read_u16)(off_t, STREAMFILE*);

    if (!check_extensions(sf, "sbr"))
        goto fail;

    /* Logically, big endian version starts with SBbe. However, this format is
     * only used on 8th gen systems so far so big endian version probably doesn't exist. */
    if (read_32bitBE(0x00, sf) == 0x53426C65) { /* "SBle" */
        read_u32 = read_u32le;
        read_u16 = read_u16le;
#if 0
    } else if (read_32bitBE(0x00, sf) == 0x53426265) { /* "SBbe" */
        read_32bit = read_u32be;
        read_16bit = read_u16be;
#endif
    } else {
        goto fail;
    }

    num_dsets = read_u16(0x0a, sf);
    data_offset = read_u32(0x20, sf);
    table_offset = read_u32(0x24, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    total_sounds = 0;
    sound_offset = 0;

    /* The bank is split into DSET sections each of which references one or multiple sounds. */
    /* Each set can contain RAM sounds (stored in SBR in data section) or streamed sounds (stored separately in SBS file). */
    for (i = 0; i < num_dsets; i++) {
        dset_offset = read_u32(table_offset + 0x08 * i, sf);
        if (read_u32(dset_offset, sf) != 0x44534554) /* "DSET" */
            goto fail;

        set_sounds = read_u32(dset_offset + 0x38, sf);
        local_target = target_stream - total_sounds - 1;
        dset_offset += 0x48;

        /* Find RAM or OFF chunk */
        while(1) {
            chunk_id = read_u32(dset_offset, sf);
            if (chunk_id == 0x2E52414D) { /* ".RAM" */
                break;
            } else if (chunk_id == 0x2E4F4646) { /* ".OFF" */
                break;
            } else if (chunk_id == 0x2E4C4452 || /* ".LDR" */
                chunk_id == 0x2E4F424A || /* ".OBJ" */
                chunk_id == 0x2E445552 || /* ".DUR" */
                (chunk_id & 0xFF00FFFF) == 0x2E00534C) { /* ".?SL */
                dset_offset += 0x18;
            } else {
                goto fail;
            }
        }

        /* Different set types store offsets differently */
        set_type = read_u8(dset_offset + 0x05, sf);

        if (set_type == 0x00) {
            total_sounds++;
            if (local_target < 0 || local_target > 0)
                continue;

            sound_offset = read_u32(dset_offset + 0x08, sf);
        } else if (set_type == 0x01) {
            total_sounds += 2;
            if (local_target < 0 || local_target > 1)
                continue;

            base_offset = read_u32(dset_offset + 0x08, sf);

            if (local_target == 0) {
                sound_offset = base_offset;
            } else {
                sound_offset = base_offset + read_u16(dset_offset + 0x06, sf);
            }
        } else if (set_type == 0x02) {
            flag = read_u8(dset_offset + 0x06, sf);
            offset_size = read_u8(dset_offset + 0x07, sf);
            base_offset = read_u32(dset_offset + 0x08, sf);
            sound_table_offset = read_u32(dset_offset + 0x10, sf);

            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            if (offset_size == 0x01) {
                sound_offset = read_u8(sound_table_offset + 0x01 * local_target, sf);
                for (j = 0; j < flag; j++) sound_offset *= 2;
            } else if (offset_size == 0x02) {
                sound_offset = read_u16(sound_table_offset + 0x02 * local_target, sf);
                for (j = 0; j < flag; j++) sound_offset *= 2;
            } else if (offset_size == 0x04) {
                sound_offset = read_u32(sound_table_offset + 0x04 * local_target, sf);
            }

            sound_offset += base_offset;
        } else if (set_type == 0x03) {
            offset_size = read_u8(dset_offset + 0x07, sf);
            set_sounds = read_u32(dset_offset + 0x08, sf);
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
            }
        } else if (set_type == 0x04) {
            total_sounds += set_sounds;
            if (local_target < 0 || local_target >= set_sounds)
                continue;

            sound_table_offset = read_u32(dset_offset + 0x10, sf);
            sound_offset = read_u32(sound_table_offset + 0x08 * local_target, sf);
        } else {
            goto fail;
        }

        snprintf(sound_name, STREAM_NAME_SIZE, "DSET %02d/%04d", i, local_target);

        if (chunk_id == 0x2E52414D) { /* ".RAM" */
            is_streamed = 0;
        } else if (chunk_id == 0x2E4F4646) { /* ".OFF" */
            is_streamed = 1;
        }
    }

    if (sound_offset == 0)
        goto fail;

    if (!is_streamed) {
        /* RAM asset */
        if (read_32bitBE(data_offset, sf) != 0x64617461) /* "data" */
            goto fail;

        sf_data = sf;
        sound_offset += data_offset;
    } else {
        /* streamed asset */
        sbsFile = open_streamfile_by_ext(sf, "sbs");
        if (!sbsFile)
            goto fail;

        if (read_32bitBE(0x00, sbsFile) != 0x64617461) /* "data" */
            goto fail;

        sf_data = sbsFile;

        if (read_32bitBE(sound_offset, sf_data) == 0x736C6F74) {
            /* skip "slot" section */
            sound_offset += 0x30;
        }
    }

    vgmstream = init_vgmstream_eaaudiocore_header(sf_data, NULL, sound_offset, 0x00, meta_EA_SPS, 0);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    strncpy(vgmstream->stream_name, sound_name, STREAM_NAME_SIZE);
    close_streamfile(sbsFile);
    return vgmstream;

fail:
    close_streamfile(sbsFile);
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

static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE *sf_data, eaac_header *eaac);
static layered_layout_data* build_layered_eaaudiocore(STREAMFILE* sf, eaac_header *eaac, off_t start_offset);
static STREAMFILE *setup_eaac_streamfile(eaac_header *ea, STREAMFILE* sf_head, STREAMFILE* sf_data);
static size_t calculate_eaac_size(STREAMFILE* sf, eaac_header *ea, uint32_t num_samples, off_t start_offset, int is_ram);

/* EA newest header from RwAudioCore (RenderWare?) / EAAudioCore library (still generated by sx.exe).
 * Audio "assets" come in separate RAM headers (.SNR/SPH) and raw blocked streams (.SNS/SPS),
 * or together in pseudoformats (.SNU, .SBR+.SBS banks, .AEMS, .MUS, etc).
 * Some .SNR include stream data, while .SPS have headers so .SPH is optional. */
static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE* sf_head, STREAMFILE* sf_data, off_t header_offset, off_t start_offset, meta_t meta_type, int standalone) {
    VGMSTREAM * vgmstream = NULL;
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
    eaac.loop_flag      = (header2 >> 29) & 0x01; /* 1 bits */
    eaac.num_samples    = (header2 >>  0) & 0x1FFFFFFF; /* 29 bits */
    /* rest is optional, depends on used flags and codec (handled below) */

    /* common channel configs are mono/stereo/quad/5.1/7.1 (from debug strings), while others are quite rare
     * [Battlefield 4 (X360)-EAXMA: 3/5/7ch, Army of Two: The Devil's Cartel (PS3)-EALayer3v2P: 11ch] */
    eaac.channels = eaac.channel_config + 1;
    /* EA 6ch channel mapping is L C R BL BR LFE, but may use stereo layers for dynamic music
     * instead, so we can't re-map automatically (use TXTP) */

    /* V0: SNR+SNS, V1: SPR+SPS (no apparent differences, other than block flags) */
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
            eaac.codec == EAAC_CODEC_XAS1)) {
            VGM_LOG("EA EAAC: unknown actual looping for codec %x\n", eaac.codec);
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
            if (eaac.loop_flag) {
                VGM_LOG("EAAC: Looped gigasample found.\n");
                goto fail;
            }
            header_size += 0x04;
            eaac.prefetch_samples = read_32bitBE(header_offset + 0x08, sf_head);
            break;
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
            if (eaac.version == EAAC_VERSION_V1) {
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
            snsFile = open_streamfile_by_ext(sf_head, "sns");
            sf_data = snsFile;
        }
        if (!sf_data) goto fail;
    }

    /* build streamfile with audio data */
    sf = setup_eaac_streamfile(&eaac, sf_head, sf_data);
    if (!sf) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(eaac.channels,eaac.loop_flag);
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
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, &eaac);
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
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, &eaac);
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
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(sf, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            }
            else {
                temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00);
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

#ifdef VGM_USE_ATRAC9
        case EAAC_CODEC_EATRAX: { /* EATrax (unknown FourCC) [Need for Speed: Most Wanted (Vita)] */
            atrac9_config cfg = {0};

            /* EATrax is "buffered" ATRAC9, uses custom IO since it's kind of complex to add to the decoder */

            cfg.channels = eaac.channels;
            /* sub-header after normal header */
            cfg.config_data = read_32bitBE(header_offset + header_size + 0x00,sf_head);
            /* 0x04: data size without blocks, LE b/c why make sense (but don't use it in case of truncated files) */
            /* 0x08: 16b frame size (same as config data) */

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00);
            if (!temp_sf) goto fail;

            break;
        }
#endif


#ifdef VGM_USE_MPEG
        case EAAC_CODEC_EAMP3: { /* "EM30"?: EAMP3 [Need for Speed 2015 (PS4)] */
            mpeg_custom_config cfg = {0};

            temp_sf = setup_eaac_audio_streamfile(sf, eaac.version, eaac.codec, eaac.streamed,0,0, 0x00);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_mpeg_custom(temp_sf, 0x00, &vgmstream->coding_type, vgmstream->channels, MPEG_EAMP3, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }

#endif

#ifdef VGM_USE_FFMPEG
        case EAAC_CODEC_EAOPUS: { /* "Eop0"? : EAOpus [FIFA 17 (PC), FIFA 19 (Switch)]*/
            vgmstream->layout_data = build_layered_eaaudiocore(sf, &eaac, 0x00);
            if (!vgmstream->layout_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_layered;
            break;
        }
#endif

        case EAAC_CODEC_EASPEEX: /* "Esp0"?: EASpeex (libspeex variant, base versions vary: 1.0.5, 1.2beta3) */ //todo
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

        if (is_ram) {
            /* RAM data only consists of one block (two for looped sounds) */
            if (ea->loop_start > 0 && !looped) looped = 1;
            else break;
        } else if (ea->version == EAAC_VERSION_V0 && block_id == EAAC_BLOCKID0_END) {
            if (ea->loop_offset > 0 && !looped) looped = 1;
            else break;
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
        }
    } else {
        if (ea->type == EAAC_TYPE_GIGASAMPLE) {
            /* not seen so far, need samples */
            VGM_LOG("EAAC: Found SPS gigasample\n");
            goto fail;
        }

        data_size = calculate_eaac_size(sf_head, ea, ea->num_samples, ea->stream_offset, ea->type == EAAC_TYPE_RAM);
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
static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE *sf_data, eaac_header *eaac) {
    segmented_layout_data *data = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t offsets[2] = { 0x00, eaac->loop_offset };
    off_t start_offset;
    int num_samples[2] = { eaac->loop_start, eaac->num_samples - eaac->loop_start};
    int segment_count = 2; /* intro/loop */
    int i;


    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    for (i = 0; i < segment_count; i++) {
        data->segments[i] = allocate_vgmstream(eaac->channels, 0);
        if (!data->segments[i]) goto fail;
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

                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version,eaac->codec,eaac->streamed,0,0, offsets[i]);
                if (!temp_sf) goto fail;

                data->segments[i]->codec_data = init_mpeg_custom(temp_sf, 0x00, &data->segments[i]->coding_type, eaac->channels, type, &cfg);
                if (!data->segments[i]->codec_data) goto fail;
                data->segments[i]->layout_type = layout_none;
                break;
            }
#endif
            default:
                goto fail;
        }

        if (!vgmstream_open_stream(data->segments[i],temp_sf == NULL ? sf_data : temp_sf, start_offset))
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

#ifdef VGM_USE_FFMPEG
        switch(eaac->codec) {
            /* EA-XMA uses completely separate 1/2ch streams, unlike standard XMA that interleaves 1/2ch
             * streams with a skip counter to reinterleave (so EA-XMA streams don't have skips set) */
            case EAAC_CODEC_EAXMA: {
                uint8_t buf[0x100];
                int bytes, block_size, block_count;
                size_t stream_size;
                int is_xma1;

                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,i,layers, start_offset);
                if (!temp_sf) goto fail;

                stream_size = get_streamfile_size(temp_sf);
                block_size = 0x10000; /* unused */
                block_count = stream_size / block_size + (stream_size % block_size ? 1 : 0);

                /* EA adopted XMA2 when it appeared around 2006, but detection isn't so easy
                 * (SNS with XMA2 do exist). Decoder should work when playing XMA1 as XMA2, but
                 * the other way around can cause issues, so it's safer to just use XMA2. */
                is_xma1 = 0; //eaac->version == EAAC_VERSION_V0; /* approximate */
                if (is_xma1)
                    bytes = ffmpeg_make_riff_xma1(buf, 0x100, data->layers[i]->num_samples, stream_size, data->layers[i]->channels, data->layers[i]->sample_rate, 0);
                else
                    bytes = ffmpeg_make_riff_xma2(buf, 0x100, data->layers[i]->num_samples, stream_size, data->layers[i]->channels, data->layers[i]->sample_rate, block_count, block_size);
                data->layers[i]->codec_data = init_ffmpeg_header_offset(temp_sf, buf,bytes, 0x00, stream_size);
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
                temp_sf = setup_eaac_audio_streamfile(sf_data, eaac->version, eaac->codec, eaac->streamed,i,layers, start_offset);
                if (!temp_sf) goto fail;

                skip = ea_opus_get_encoder_delay(0x00, temp_sf);
                data_size = get_streamfile_size(temp_sf);

                data->layers[i]->codec_data = init_ffmpeg_ea_opus(temp_sf, 0x00,data_size, layer_channels, skip, eaac->sample_rate);
                if (!data->layers[i]->codec_data) goto fail;
                data->layers[i]->coding_type = coding_FFmpeg;
                data->layers[i]->layout_type = layout_none;
                break;
            }

        }
#else
        goto fail;
#endif

        if ( !vgmstream_open_stream(data->layers[i], temp_sf, 0x00) ) {
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
