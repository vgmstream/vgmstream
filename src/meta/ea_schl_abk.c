#include "meta.h"
#include "../layout/layout.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

#define EA_BLOCKID_HEADER           0x5343486C /* "SCHl" */

#define EA_BNK_HEADER_LE            0x424E4B6C /* "BNKl" */
#define EA_BNK_HEADER_BE            0x424E4B62 /* "BNKb" */

static VGMSTREAM* init_vgmstream_ea_abk_schl_main(STREAMFILE* sf);

/* .ABK - standard */
VGMSTREAM* init_vgmstream_ea_abk_schl(STREAMFILE* sf) {
    if (!check_extensions(sf, "abk"))
        return NULL;
    return init_vgmstream_ea_abk_schl_main(sf);
}

/* .AMB/AMX - EA Redwood Shores variant [007: From Russia with Love, The Godfather (PC/PS2/Xbox/Wii)] */
VGMSTREAM* init_vgmstream_ea_amb_schl(STREAMFILE* sf) {
    /* container with .ABK ("ABKC") and .CSI ("MOIR") data */
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_abk = NULL;
    off_t abk_offset, header_offset = 0x00;
    size_t abk_size;
    uint32_t version;
    read_u32_t read_u32;

    if (!check_extensions(sf, "amb,amx"))
        return NULL;

    /* 0x08 covers both v4-v8 and v9 */
    read_u32 = guess_read_u32(0x08, sf);

    if (read_u64le(0x00, sf) == 0) {
        header_offset = read_u32(0x08, sf);
        if (header_offset > 0x40) /* 0x20 (v4), 0x30/0x40 (v8) */
            return NULL;
        version = read_u32(header_offset, sf);
        if (version != 0x04 && version != 0x08)
            return NULL;
    }
    else if (read_u32(0x00, sf) != 0x09)
        return NULL;


    version = read_u32(header_offset + 0x00, sf);

    abk_offset = header_offset + (version > 0x04 ? 0x40 : 0x20);
    /* Version 0x04: [007: From Russia with Love (Demo)]
     *  0x04: MOIR offset (+ abk offset)
     *  0x08: unk (always 0?)
     *  0x0C: unk (some hash?)
     *  0x10: always 2.0f?
     *  0x14: always 2.0f?
     *  0x18: always 100.0f?
     *  0x1C: unk (some bools? 0x0101)
     *  0x20: ABKC data
     */
    /* Version 0x08: [007: From Russia with Love]
     *  0x04: MOIR offset (+ abk offset)
     *  0x08: unk offset (same as MOIR)
     *  0x0C: unk (always 0?)
     *  0x10: unk (some hash?)
     *  0x14: usually 2.0f? sometimes 1.0f or 5.0f
     *  0x18: always 2.0f?
     *  0x1C: usually 75.0f? sometimes 100.0f or 1000.0f
     *  0x20: unk (some bools? 0x0101)
     *  0x40: ABKC data
     */
    /* Version 0x09: [The Godfather]
     *  0x04: MOIR offset (+ abk_offset)
     *  0x08: MOIR size
     *  0x0C: unk offset (same as MOIR, usually)
     *  0x10: unk size (always 0?)
     *  0x14: unk (some hash?)
     *  0x18: always 1.0f?
     *  0x1C: always 2.0f?
     *  0x20: always 100.0f?
     *  0x24: unk (some bools? sometimes 0x010000)
     *  0x40: ABKC data
     */
    abk_size = read_u32(header_offset + 0x04, sf);

    if (abk_size > get_streamfile_size(sf))
        goto fail;

    /* in case stricter checks are needed: */
    //if (!is_id32be(abk_offset + abk_size, sf, "MOIR"))
    //    goto fail;

    /* (v9) rarely some other bad(?) value [The Godfather (PS2)] */
    //if (read_u32(0x0C, sf) != abk_size) goto fail;

    sf_abk = open_wrap_streamfile(sf);
    sf_abk = open_clamp_streamfile(sf_abk, abk_offset, abk_size);
    if (!sf_abk) goto fail;

    vgmstream = init_vgmstream_ea_abk_schl_main(sf_abk);
    if (!vgmstream) goto fail;

    close_streamfile(sf_abk);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_abk);
    return NULL;
}

/* EA ABK - common soundbank format in 6th-gen games, can reference RAM and streamed assets */
/* RAM assets are stored in embedded BNK file */
/* streamed assets are stored externally in AST file (mostly seen in earlier 6th-gen games) */
static VGMSTREAM* init_vgmstream_ea_abk_schl_main(STREAMFILE* sf) {
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
