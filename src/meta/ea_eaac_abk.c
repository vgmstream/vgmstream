#include "meta.h"
#include "../util/endianness.h"

static VGMSTREAM* init_vgmstream_ea_abk_eaac_main(STREAMFILE* sf);
static VGMSTREAM* parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t ast_offset);

/* .ABK - standard */
VGMSTREAM* init_vgmstream_ea_abk_eaac(STREAMFILE* sf) {
    if (!check_extensions(sf, "abk"))
        return NULL;
    return init_vgmstream_ea_abk_eaac_main(sf);
}

/* .AMB/AMX - EA Redwood Shores variant [The Godfather (PS3/X360), The Simpsons Game (PS3/360)] */
VGMSTREAM* init_vgmstream_ea_amb_eaac(STREAMFILE* sf) {
    /* container with .ABK ("ABKC") and .CSI ("MOIR") data */
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_abk = NULL;
    off_t abk_offset;
    size_t abk_size;
    read_u32_t read_u32;

    if (!check_extensions(sf, "amb,amx"))
        return NULL;

    read_u32 = guess_read_u32(0x00, sf);
    if (read_u32(0x00, sf) != 0x09) /* version */
        return NULL;

    abk_offset = 0x40;
    /* 0x04: MOIR offset (+ abk_offset)
     * 0x08: MOIR size
     * 0x0C: unk offset (same as MOIR)
     * 0x10: unk size (always 0?)
     * 0x14: unk (some hash?)
     * 0x18: always 1.0f?
     * 0x1C: always 2.0f?
     * 0x20: always 100.0f?
     * 0x24: unk (some bitfield? sometimes 0x10000)
     */
    abk_size = read_u32(0x04, sf);

    if (read_u32(0x0C, sf) != abk_size)
        goto fail;

    /* in case stricter checks are needed: */
    //if (!is_id32be(abk_offset + abk_size, sf, "MOIR"))
    //    goto fail;

    sf_abk = open_wrap_streamfile(sf);
    sf_abk = open_clamp_streamfile(sf_abk, abk_offset, abk_size);
    if (!sf_abk) goto fail;

    vgmstream = init_vgmstream_ea_abk_eaac_main(sf_abk);
    if (!vgmstream) goto fail;

    close_streamfile(sf_abk);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_abk);
    return NULL;
}

/* EA ABK - ABK header seems to be same as in the old games but the sound table is different and it contains SNR/SNS sounds instead */
static VGMSTREAM* init_vgmstream_ea_abk_eaac_main(STREAMFILE* sf) {
    VGMSTREAM* vgmstream;
    int is_dupe, total_sounds = 0, target_stream = sf->stream_index;
    off_t bnk_offset, modules_table, module_data, player_offset, samples_table, entry_offset, ast_offset;
    off_t cfg_num_players_off, cfg_module_data_off, cfg_module_entry_size, cfg_samples_table_off;
    uint32_t num_sounds, num_sample_tables;
    uint16_t num_modules, bnk_index, bnk_target_index;
    uint8_t num_players;
    off_t sample_tables[0x400];
    read_u32_t read_u32;
    read_u16_t read_u16;


    /* checks */
    if (!is_id32be(0x00, sf, "ABKC"))
        return NULL;

    /* use table offset to check endianness */
    if (guess_endian32(0x1C, sf)) {
        read_u32 = read_u32be;
        read_u16 = read_u16be;
    } else {
        read_u32 = read_u32le;
        read_u16 = read_u16le;
    }

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    num_modules = read_u16(0x0A, sf);
    modules_table = read_u32(0x1C, sf);
    bnk_offset = read_u32(0x20, sf);
    num_sample_tables = 0;
    bnk_target_index = 0xFFFF;
    ast_offset = 0;

    if (!bnk_offset || !is_id32be(bnk_offset, sf, "S10A"))
        goto fail;

    /* set up some common values */
    if (modules_table == 0x5C) {
        /* the usual variant */
        cfg_num_players_off = 0x24;
        cfg_module_data_off = 0x2C;
        cfg_module_entry_size = 0x3C;
        cfg_samples_table_off = 0x04;
    }
    else if (modules_table == 0x78) {
        /* FIFA 08 has a bunch of extra zeroes all over the place, don't know what's up with that */
        cfg_num_players_off = 0x40;
        cfg_module_data_off = 0x54;
        cfg_module_entry_size = 0x68;
        cfg_samples_table_off = 0x0C;
    }
    else {
        goto fail;
    }

    for (uint32_t i = 0; i < num_modules; i++) {
        num_players = read_u8(modules_table + cfg_num_players_off, sf);
        module_data = read_u32(modules_table + cfg_module_data_off, sf);
        if (num_players == 0xff) goto fail; /* EOF read */

        for (uint32_t j = 0; j < num_players; j++) {
            player_offset = read_u32(modules_table + cfg_module_entry_size + 0x04 * j, sf);
            samples_table = read_u32(module_data + player_offset + cfg_samples_table_off, sf);

            /* multiple players may point at the same sound table */
            is_dupe = 0;
            for (uint32_t k = 0; k < num_sample_tables; k++) {
                if (samples_table == sample_tables[k]) {
                    is_dupe = 1;
                    break;
                }
            }

            if (is_dupe)
                continue;

            sample_tables[num_sample_tables++] = samples_table;
            num_sounds = read_u32(samples_table, sf);
            if (num_sounds == 0xffffffff) goto fail; /* EOF read */

            for (uint32_t k = 0; k < num_sounds; k++) {
                /* 0x00: sound index */
                /* 0x02: priority */
                /* 0x03: azimuth */
                /* 0x08: streamed data offset */
                entry_offset = samples_table + 0x04 + 0x0C * k;
                bnk_index = read_u16(entry_offset + 0x00, sf);

                /* some of these are dummies */
                if (bnk_index == 0xFFFF)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds) {
                    bnk_target_index = bnk_index;
                    ast_offset = read_u32(entry_offset + 0x08, sf);
                }
            }
        }

        /* skip class controllers */
        num_players += read_u8(modules_table + cfg_num_players_off + 0x03, sf);
        modules_table += cfg_module_entry_size + num_players * 0x04;
    }

    if (bnk_target_index == 0xFFFF || ast_offset == 0)
        goto fail;

    vgmstream = parse_s10a_header(sf, bnk_offset, bnk_target_index, ast_offset);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_sounds;
    return vgmstream;

fail:
    return NULL;
}

/* EA S10A header - seen inside new ABK files. Putting it here in case it's encountered stand-alone. */
static VGMSTREAM* parse_s10a_header(STREAMFILE* sf, off_t offset, uint16_t target_index, off_t sns_offset) {
    VGMSTREAM* vgmstream;
    STREAMFILE *sf_ast = NULL;
    uint32_t num_sounds;
    off_t snr_offset;
    eaac_meta_t info = {0};

    /* header is always big endian */
    /* 0x00: header magic */
    /* 0x04: version */
    /* 0x05: padding */
    /* 0x06: serial number */
    /* 0x08: number of files */
    /* 0x0C: offsets table */
    if (!is_id32be(offset + 0x00, sf, "S10A"))
        return NULL;

    num_sounds = read_u32be(offset + 0x08, sf);
    if (num_sounds == 0 || target_index >= num_sounds)
        return NULL;

    snr_offset = offset + read_u32be(offset + 0x0C + 0x04 * target_index, sf);

    if (sns_offset == 0xFFFFFFFF) {
        /* RAM asset */
        //;VGM_LOG("EA S10A: RAM at snr=%lx", snr_offset);

        info.sf_head = sf;
        info.head_offset = snr_offset;
        info.body_offset = 0x00;
        info.type = meta_EA_SNR_SNS;

        vgmstream = load_vgmstream_ea_eaac(&info);
        if (!vgmstream) goto fail;
    }
    else {
        /* streamed asset */
        sf_ast = open_streamfile_by_ext(sf, "ast");
        if (!sf_ast) {
            vgm_logi("EA ABK: .ast file not found (find and put together)\n");
            goto fail;
        }

        if (!is_id32be(0x00, sf_ast, "S10S"))
            goto fail;

        //;VGM_LOG("EA S10A: stream at snr=%lx, sns=%lx\n", snr_offset, sns_offset);

        info.sf_head = sf;
        info.sf_body = sf_ast;
        info.head_offset = snr_offset;
        info.body_offset = sns_offset;
        info.type = meta_EA_SNR_SNS;

        vgmstream = load_vgmstream_ea_eaac(&info);
        if (!vgmstream) goto fail;

        close_streamfile(sf_ast);
    }

    return vgmstream;
fail:
    close_streamfile(sf_ast);
    return NULL;
}
