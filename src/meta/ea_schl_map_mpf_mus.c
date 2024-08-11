#include "meta.h"
#include "../util/endianness.h"
#include "../layout/layout.h"
#include "../util/companion_files.h"
#include "../util/layout_utils.h"

#define EA_BLOCKID_HEADER           0x5343486C /* "SCHl" */

#define EA_BNK_HEADER_LE            0x424E4B6C /* "BNKl" */
#define EA_BNK_HEADER_BE            0x424E4B62 /* "BNKb" */

static VGMSTREAM* init_vgmstream_ea_mpf_mus_schl_main(STREAMFILE* sf, const char* mus_name);
static STREAMFILE* open_mapfile_pair(STREAMFILE* sf, int track /*, int num_tracks*/);

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
        return NULL;

    /* always big endian */
    if (!is_id32be(0x00, sf, "PFDx"))
        return NULL;

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

    vgmstream = load_vgmstream_ea_schl(sf_mus, schl_offset);
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

/* .MPF - Standard EA MPF+MUS */
VGMSTREAM* init_vgmstream_ea_mpf_mus_schl(STREAMFILE* sf) {
    if (!check_extensions(sf, "mpf"))
        return NULL;
    return init_vgmstream_ea_mpf_mus_schl_main(sf, NULL);
}

/* .MSB/MSX - EA Redwood Shores (MSB/MSX)+MUS [007: From Russia with Love, The Godfather (PC/PS2/Xbox/Wii)] */
VGMSTREAM* init_vgmstream_ea_msb_mus_schl(STREAMFILE* sf) {
    /* container with .MPF ("PFDx"), .NAM (numevents/numparts/numdefines), and a pre-defined MUS filename */
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_mpf = NULL;
    char mus_name[0x20 + 1];
    size_t mpf_size;
    off_t header_offset, mpf_offset, mus_name_offset;
    read_u32_t read_u32;

    if (!check_extensions(sf, "msb,msx"))
        return NULL;

    read_u32 = guess_read_u32(0x08, sf);

    if (read_u64le(0x00, sf) != 0)
        return NULL;
    header_offset = read_u32(0x08, sf);
    if (header_offset != 0x20)
        return NULL;
    if (read_u32(header_offset, sf) != 0x05) /* version */
        return NULL;


    mpf_offset = header_offset + 0x30;
    /* Version 0x05:
     *  0x04: plaintext events/parts/defines offset (+ mpf offset)
     *  0x08: always 0?
     *  0x0C: some hash?
     *  0x10: intended .mus filename 
     *  0x30: PFDx data
     */
    mus_name_offset = header_offset + 0x10;

    /* not exactly the same as mpf size since it's aligned, but correct size is only needed for v3 */
    mpf_size = read_u32(header_offset + 0x04, sf);
    /* TODO: it should be theoretically possible to use the numparts section
     * in the plaintext chunk to get valid stream names using its entry node
     * indices by checking if they're within range of the current subsong */
    read_string(mus_name, sizeof(mus_name), mus_name_offset, sf);

    sf_mpf = open_wrap_streamfile(sf);
    sf_mpf = open_clamp_streamfile(sf_mpf, mpf_offset, mpf_size);
    if (!sf_mpf) goto fail;

    vgmstream = init_vgmstream_ea_mpf_mus_schl_main(sf_mpf, mus_name);
    if (!vgmstream) goto fail;

    close_streamfile(sf_mpf);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_mpf);
    return NULL;
}

/* EA MPF/MUS combo - used in 6th gen games for interactive music (for EA's PathFinder tool) */
static VGMSTREAM* init_vgmstream_ea_mpf_mus_schl_main(STREAMFILE* sf, const char* mus_name) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_mus = NULL;
    segmented_layout_data* data_s = NULL;
    uint32_t tracks_table, tracks_data, samples_table = 0, section_offset, entry_offset = 0, eof_offset = 0, sound_offset,
        off_mult = 0, track_start, track_end = 0, track_checksum = 0;
    uint16_t num_nodes, num_subbanks = 0;
    uint8_t version, sub_version, num_tracks, num_sections, num_events, num_routers, num_vars, subentry_num = 0;
    int i;
    int target_stream = sf->stream_index, total_streams, big_endian, is_ram = 0;
    read_u32_t read_u32;
    read_u16_t read_u16;

    /* detect endianness */
    if (is_id32be(0x00, sf, "PFDx")) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
        big_endian = 1;
    }
    else if (is_id32le(0x00, sf, "PFDx")) {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
        big_endian = 0;
    }
    else {
        return NULL;
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
        /* SSX Tricky, Sled Storm */ {
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
    }
    else if (version == 3 && sub_version == 4)
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
    }
    else if (version == 4) {
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
    }
    else if (version == 5) {
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
    sf_mus = mus_name ? open_streamfile_by_filename(sf, mus_name) : open_mapfile_pair(sf, i); //, num_tracks
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
            data_s->segments[i] = load_vgmstream_ea_bnk(sf_mus, bnk_offset, bnk_sound_index + i, 1);
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

        vgmstream = load_vgmstream_ea_schl(sf_mus, sound_offset);
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

/* open map/mpf+mus pairs that aren't exact pairs, since EA's games can load any combo */
static STREAMFILE* open_mapfile_pair(STREAMFILE* sf, int track /*, int num_tracks*/) {
    static const char* const mapfile_pairs[][2] = {
        /* standard cases, replace map part with mus part (from the end to preserve prefixes) */
        {"MUS_CTRL.MPF",    "MUS_STR.MUS"}, /* GoldenEye - Rogue Agent (PS2) */
        {"mus_ctrl.mpf",    "mus_str.mus"}, /* GoldenEye - Rogue Agent (others) */
        {"AKA_Mus.mpf",     "Track.mus"},   /* Boogie */
        {"SSX4FE.mpf",      "TrackFE.mus"}, /* SSX On Tour */
        {"SSX4Path.mpf",    "Track.mus"},
        {"SSX4.mpf",        "moments0.mus,main.mus,load_loop0.mus"}, /* SSX Blur */
        {"*.mpf",           "*_main.mus"},  /* 007: Everything or Nothing */
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
        const char* map_name = mapfile_pairs[i][0];
        const char* mus_name = mapfile_pairs[i][1];
        char buf[PATH_LIMIT] = { 0 };
        char* pch;
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
        char* mod_name = strchr(file_name, '+');
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
