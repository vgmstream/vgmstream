#include "meta.h"
#include "../layout/layout.h"
#include "../util/endianness.h"

#define EA_BLOCKID_HEADER           0x5343486C /* "SCHl" */

#define EA_BNK_HEADER_LE            0x424E4B6C /* "BNKl" */
#define EA_BNK_HEADER_BE            0x424E4B62 /* "BNKb" */

/* EA ABK - common soundbank format in 6th-gen games, can reference RAM and streamed assets */
/* RAM assets are stored in embedded BNK file */
/* streamed assets are stored externally in AST file (mostly seen in earlier 6th-gen games) */
VGMSTREAM* init_vgmstream_ea_abk_schl(STREAMFILE* sf) {
    int bnk_target_stream, is_dupe, total_sounds = 0, target_stream = sf->stream_index;
    off_t bnk_offset, modules_table, module_data, player_offset, samples_table, entry_offset, target_entry_offset, schl_offset, schl_loop_offset;
    uint32_t i, j, k, num_sounds, num_sample_tables;
    uint16_t num_modules;
    uint8_t sound_type, num_players;
    off_t sample_tables[0x400];
    STREAMFILE* sf_ast = NULL;
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data_s = NULL;
    read_u32_t read_u32;
    read_u16_t read_u16;

    /* check extension */
    if (!is_id32be(0x00, sf, "ABKC"))
        return NULL;
    if (!check_extensions(sf, "abk"))
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
    target_entry_offset = 0;
    num_sample_tables = 0;

    /* check to avoid clashing with the newer ABK format */
    if (bnk_offset &&
        read_u32be(bnk_offset, sf) != EA_BNK_HEADER_LE &&
        read_u32be(bnk_offset, sf) != EA_BNK_HEADER_BE)
        goto fail;

    for (i = 0; i < num_modules; i++) {
        num_players = read_u8(modules_table + 0x24, sf);
        module_data = read_u32(modules_table + 0x2C, sf);
        if (num_players == 0xff) goto fail; /* EOF read */

        for (j = 0; j < num_players; j++) {
            player_offset = read_u32(modules_table + 0x3C + 0x04 * j, sf);
            samples_table = read_u32(module_data + player_offset + 0x04, sf);

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
            num_sounds = read_u32(samples_table, sf);
            if (num_sounds == 0xffffffff) goto fail; /* EOF read */

            for (k = 0; k < num_sounds; k++) {
                entry_offset = samples_table + 0x04 + 0x0C * k;
                sound_type = read_u8(entry_offset + 0x00, sf);

                /* some of these are dummies pointing at sound 0 in BNK */
                if (sound_type == 0x00 && read_u32(entry_offset + 0x04, sf) == 0)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds)
                    target_entry_offset = entry_offset;
            }
        }

        /* skip class controllers */
        num_players += read_u8(modules_table + 0x27, sf);
        modules_table += 0x3C + num_players * 0x04;
    }

    if (target_entry_offset == 0)
        goto fail;

    /* 0x00: type (0x00 - RAM, 0x01 - streamed, 0x02 - streamed looped) */
    /* 0x01: priority */
    /* 0x02: padding */
    /* 0x04: index for RAM sounds, offset for streamed sounds */
    /* 0x08: loop offset for streamed sounds */
    sound_type = read_u8(target_entry_offset + 0x00, sf);

    switch (sound_type) {
        case 0x00:
            if (!bnk_offset)
                goto fail;

            bnk_target_stream = read_u32(target_entry_offset + 0x04, sf);
            vgmstream = load_vgmstream_ea_bnk(sf, bnk_offset, bnk_target_stream, 1);
            if (!vgmstream)
                goto fail;

            break;

        case 0x01:
            sf_ast = open_streamfile_by_ext(sf, "ast");
            if (!sf_ast)
                goto fail;

            schl_offset = read_u32(target_entry_offset + 0x04, sf);
            if (read_u32be(schl_offset, sf_ast) != EA_BLOCKID_HEADER)
                goto fail;

            vgmstream = load_vgmstream_ea_schl(sf_ast, schl_offset);
            if (!vgmstream)
                goto fail;

            break;

        case 0x02:
            sf_ast = open_streamfile_by_ext(sf, "ast");
            if (!sf_ast) {
                vgm_logi("EA ABK: .ast file not found (find and put together)\n");
                goto fail;
            }

            /* looped sounds basically consist of two independent segments
             * the first one is loop start, the second one is loop body */
            schl_offset = read_u32(target_entry_offset + 0x04, sf);
            schl_loop_offset = read_u32(target_entry_offset + 0x08, sf);

            if (read_u32be(schl_offset, sf_ast) != EA_BLOCKID_HEADER ||
                read_u32be(schl_loop_offset, sf_ast) != EA_BLOCKID_HEADER)
                goto fail;

            /* init layout */
            data_s = init_layout_segmented(2);
            if (!data_s) goto fail;

            /* load intro and loop segments */
            data_s->segments[0] = load_vgmstream_ea_schl(sf_ast, schl_offset);
            if (!data_s->segments[0]) goto fail;
            data_s->segments[1] = load_vgmstream_ea_schl(sf_ast, schl_loop_offset);
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
    close_streamfile(sf_ast);
    return vgmstream;

fail:
    close_streamfile(sf_ast);
    free_layout_segmented(data_s);
    return NULL;
}
