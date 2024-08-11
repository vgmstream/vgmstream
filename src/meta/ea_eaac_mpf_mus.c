#include "meta.h"
#include "../util/endianness.h"
#include "../layout/layout.h"
#include "../util/layout_utils.h"
#include "../util/companion_files.h"


static VGMSTREAM* init_vgmstream_ea_mpf_mus_eaac_main(STREAMFILE* sf, const char* mus_name);
static STREAMFILE *open_mapfile_pair(STREAMFILE* sf, int track /*, int num_tracks*/);

/* .MPF - Standard EA MPF+MUS */
VGMSTREAM* init_vgmstream_ea_mpf_mus_eaac(STREAMFILE* sf) {
    if (!check_extensions(sf, "mpf"))
        return NULL;
    return init_vgmstream_ea_mpf_mus_eaac_main(sf, NULL);
}

/* .MSB/MSX - EA Redwood Shores (MSB/MSX)+MUS [The Godfather (PS3/360), The Simpsons Game (PS3/360)] */
VGMSTREAM* init_vgmstream_ea_msb_mus_eaac(STREAMFILE* sf) {
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
    /* However, there is something funky going on, where for each stream the
     * number goes off by 2 (and in some cases by a much larger value) which
     * needs to be worked out first before anything else can be done */
    read_string(mus_name, sizeof(mus_name), mus_name_offset, sf);
    /* usually the same base name, but can rarely be different
     * e.g. bargain_bin.msb/msx -> bin.mus [The Simpsons Game]
     */

    sf_mpf = open_wrap_streamfile(sf);
    sf_mpf = open_clamp_streamfile(sf_mpf, mpf_offset, mpf_size);
    if (!sf_mpf) goto fail;

    vgmstream = init_vgmstream_ea_mpf_mus_eaac_main(sf_mpf, mus_name);
    if (!vgmstream) goto fail;

    close_streamfile(sf_mpf);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_mpf);
    return NULL;
}

/* EA MPF/MUS combo - used in older 7th gen games for storing interactive music */
static VGMSTREAM* init_vgmstream_ea_mpf_mus_eaac_main(STREAMFILE* sf, const char* mus_name) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *sf_mus = NULL;
    uint32_t num_tracks, track_start, track_checksum = 0, mus_sounds, mus_stream = 0, bnk_index = 0, bnk_sound_index = 0,
        tracks_table, samples_table, eof_offset, table_offset, entry_offset = 0, snr_offset, sns_offset;
    uint16_t num_subbanks, index, sub_index;
    uint8_t version, sub_version;
    segmented_layout_data* data_s = NULL;
    int i;
    int target_stream = sf->stream_index, total_streams, is_ram = 0;
    read_u32_t read_u32;
    read_u16_t read_u16;

    /* checks */
    if (is_id32be(0x00, sf, "PFDx")) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    }
    else if (is_id32le(0x00, sf, "PFDx")) {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }
    else {
        return NULL;
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
    sf_mus = mus_name ? open_streamfile_by_filename(sf, mus_name) : open_mapfile_pair(sf, i);//, num_tracks
    if (!sf_mus) goto fail;

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
    }
    else {
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
                eaac_meta_t info = {0};

                entry_offset = table_offset + (bnk_sound_index + i) * 0x0c;
                snr_offset = read_u32(entry_offset + 0x04, sf_mus);

                info.sf_head = sf_mus;
                info.head_offset = snr_offset;
                info.body_offset = 0x00;
                info.type = meta_EA_SNR_SNS;

                data_s->segments[i] = load_vgmstream_ea_eaac(&info);
                if (!data_s->segments[i]) goto fail;
            }

            /* setup segmented VGMSTREAMs */
            if (!setup_layout_segmented(data_s))
                goto fail;
            vgmstream = allocate_segmented_vgmstream(data_s, 0, 0, 0);
        }
        else {
            eaac_meta_t info = {0};

            entry_offset = table_offset + mus_stream * 0x0c;
            snr_offset = read_u32(entry_offset + 0x04, sf_mus);
            sns_offset = read_u32(entry_offset + 0x08, sf_mus);

            info.sf_head = sf_mus;
            info.sf_body = sf_mus;
            info.head_offset = snr_offset;
            info.body_offset = sns_offset;
            info.type = meta_EA_SNR_SNS;

            vgmstream = load_vgmstream_ea_eaac(&info);
        }
    }
    else if (sub_version == 3) {
        eaac_meta_t info = {0};

        /* number of samples is always little endian */
        mus_sounds = read_u32le(0x04, sf_mus);
        if (mus_stream >= mus_sounds)
            goto fail;

        if (is_ram) {
            /* not seen so far */
            VGM_LOG("EA EAAC MUS: found RAM track in MPF v5.3.\n");
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

        info.sf_head = sf_mus;
        info.sf_body = sf_mus;
        info.head_offset = snr_offset;
        info.body_offset = sns_offset;
        info.type = meta_EA_SNR_SNS;

        vgmstream = load_vgmstream_ea_eaac(&info);
    }
    else {
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

//TODO remove a few lesser ones + use .txtm

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
