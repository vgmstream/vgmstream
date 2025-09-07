#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"
#include "../util/endianness.h"

/* Parses various banks of Nintendo EAD's JAudio, mainly used for sequences but also sfx
 * Loads external raw .aw, info being inside banks.
 *
 * Some info from:
 * - https://www.lumasworkshop.com/wiki/SMR.szs
 * - https://github.com/XAYRGA/JaiSeqX
 */

typedef enum { JAUDIO_AAF, JAUDIO_BX, JAUDIO_BAA } jaudio_type_t;

/* Reads WSYS info, which contains multiple .aw names and its subsongs's stream config
 */
static bool parse_jaudio_wsys(meta_header_t* hdr, STREAMFILE* sf, uint32_t wsys_offset, jaudio_type_t type) {
    read_u32_t read_u32 = hdr->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hdr->big_endian ? read_s32be : read_s32le;
    read_f32_t read_f32 = hdr->big_endian ? read_f32be : read_f32le;

    if (read_u32(wsys_offset+0x00,sf) != get_id32be("WSYS")) {
        if (hdr->big_endian)
            return false;
        // Switch LE's Pikmin 1 uses WSYS + FINW while Pikmin 2 uses SYSW + FINW
        if (read_u32be(wsys_offset+0x00,sf) != get_id32be("WSYS"))
            return false;
    }
    // 04: chunk size
    // 08: null
    // 0c: null

    uint32_t winf_offset = read_u32(wsys_offset + 0x10, sf) + wsys_offset;
    // 14: offset to WBCT (offsets to SCNEs > C-DF / C-EX / C-ST)

    if (read_u32(winf_offset+0x00, sf) != get_id32be("WINF"))
        return false;
    int aw_entries = read_s32(winf_offset+0x04, sf);
    // 08+: offsets to .aw info

    winf_offset += 0x08;

    for (int i = 0; i < aw_entries; i++) {
        uint32_t aw_offset = read_u32(winf_offset, sf) + wsys_offset;
        winf_offset += 0x04;
        if (type == JAUDIO_BX && !hdr->big_endian)
            winf_offset += 0x04; //empty (or 64-bit offset) in Pikmin 1 LE

        if (aw_offset == 0)
            return false;

        uint32_t aw_name_offset = aw_offset + 0x00;
        aw_offset += hdr->big_endian ? 0x70 : 0x8C;

        int wave_entries = read_s32(aw_offset + 0x00, sf);
        aw_offset += 0x04;
        // 04+: offsets to wave info

        for (int j = 0; j < wave_entries; j++) {
            hdr->total_subsongs++;
            if (hdr->target_subsong != hdr->total_subsongs) {
                aw_offset += 0x04;
                if (type == JAUDIO_BX && !hdr->big_endian)
                    aw_offset += 0x04; //empty (or 64-bit offset) in Pikmin 1 LE
                continue;
            }

            uint32_t wave_offset = read_u32(aw_offset, sf) + wsys_offset;
            if (type == JAUDIO_BX && !hdr->big_endian)
                aw_offset += 0x04; //empty (or 64-bit offset) in Pikmin 1 LE
            aw_offset += 0x04;

            // 00: volume? (FF: common, 5C/00: Pikmin, 70: SMG, 40: SMG2, etc)
            uint8_t format      = read_u8 (wave_offset + 0x01, sf);
            // 02: base key?
            // 03: null
            hdr->sample_rate    = read_f32(wave_offset + 0x04, sf); //float!
            hdr->stream_offset  = read_u32(wave_offset + 0x08, sf);
            hdr->stream_size    = read_u32(wave_offset + 0x0c, sf);
            hdr->loop_flag      = read_s32(wave_offset + 0x10, sf) == -1;
            hdr->loop_start     = read_s32(wave_offset + 0x14, sf); // 0 if loop not set
            hdr->loop_end       = read_s32(wave_offset + 0x18, sf); // num_samples if loop not set
            hdr->num_samples    = read_s32(wave_offset + 0x1c, sf);
            // 20: loop hist1?
            // 22: loop hist2?
            // 24: null?
            // - big endian
            // 28: garbage?
            // - little endian:
            // 28: null?
            // 2c: garbage?

            hdr->name_offset = aw_name_offset;

            switch (format) {
                case 0x00:
                    hdr->coding = coding_AFC;
                    break;
                case 0x01: // rare (ex. SMS #518, Pikmin #7)
                    hdr->coding = coding_AFC_2bit;
                    break;
                case 0x02: // uncommon (ex. WW #1402)
                    hdr->coding = coding_PCM8;
                    break;
                case 0x03: // rare (ex. SMG #906, P2 #865)
                    hdr->coding = coding_PCM16BE; //BE even on LE's Pikmin 2 (Switch)
                    break;
                default:
                    VGM_LOG("JAUDIO: unknown format %x at %x\n", format, wave_offset);
                    return false;
            }
        }
    }

    return true;
}

/* Load .aw in common dirs
 */
static STREAMFILE* load_aw(STREAMFILE* sf, uint32_t name_offset) {

    char aw_name[0x70];
    read_string(aw_name, sizeof(aw_name), name_offset, sf);

    STREAMFILE* sf_body = NULL;

    // .bx: try file as-is
    sf_body = open_streamfile_by_filename(sf, aw_name);
    if (sf_body)
        return sf_body;

    char dir_name[0x70 + 0x10];

    // .aaf: try in subdir
    snprintf(dir_name, sizeof(dir_name), "Banks/%s", aw_name);
    sf_body = open_streamfile_by_filename(sf, dir_name);
    if (sf_body)
        return sf_body;

    // .baa: try in subdir
    snprintf(dir_name, sizeof(dir_name), "Waves/%s", aw_name);
    sf_body = open_streamfile_by_filename(sf, dir_name);
    if (sf_body)
        return sf_body;

    // unknown
    return NULL;
}

static VGMSTREAM* init_vgmstream_jaudio(STREAMFILE* sf, meta_header_t* hdr) {
    if (!hdr->stream_size) {
        VGM_LOG("JAUDIO: info not found\n");
        return false;
    }

    // fill partial header
    hdr->meta = meta_JAUDIO;
    hdr->has_subsongs = true;

    hdr->channels = 1;
    hdr->layout = layout_none;

    hdr->open_stream = true;
    hdr->sf_head = sf;
    hdr->sf_body = load_aw(sf, hdr->name_offset);
    if (!hdr->sf_body) {
        hdr->coding = coding_SILENCE; // don't fail, to signal missing .aw
    }

    VGMSTREAM* vgmstream = alloc_metastream(hdr);
    close_streamfile(hdr->sf_body);
    if (hdr->coding == coding_SILENCE) {
        meta_mark_missing(vgmstream);
    }

    return vgmstream;
}

static void meta_load_subsong(meta_header_t* hdr, STREAMFILE* sf) {
    int target_subsong = sf->stream_index;
    if (target_subsong == 0)
        target_subsong = 1;
    hdr->target_subsong = target_subsong;
}

/* .aaf - Nintendo JAudio (v1?) games [Super Mario Sunshine (GC), Luigi's Mansion (GC), Zelda: Wind Waker (GC), Pikmin 2 (GC/Switch)] */
VGMSTREAM* init_vgmstream_jaudio_aaf(STREAMFILE* sf) {

    /* checks */
    if ((read_u32be(0x00, sf) != 0x01 || read_u32be(0x0c, sf) != 0x00 || read_u32be(0x10, sf) != 0x02) && 
        (read_u32le(0x00, sf) != 0x01 || read_u32le(0x0c, sf) != 0x00 || read_u32le(0x10, sf) != 0x02)) // expected chunks
        return NULL;
    if (!check_extensions(sf, "aaf"))
        return NULL;


    meta_header_t hdr = {0};
    meta_load_subsong(&hdr, sf);
    hdr.big_endian = guess_endian32(0x00, sf); // Pikmin 2 Switch uses LE, but not SMS/SMG in Mario 3D All-Stars

    read_u32_t read_u32 = hdr.big_endian ? read_u32be : read_u32le;

    // parse chunks: N chunk ids each with offset + size + config (repeatable in some cases)
    uint32_t max_offset = get_streamfile_size(sf);
    uint32_t offset = 0x00;
    while (offset < max_offset) {
        uint32_t chunk_id = read_u32(offset, sf);
        offset += 0x04;
        if (chunk_id == 0) // end chunk
            break;

        switch(chunk_id) {
            case 0x01:      // finetune table?
            case 0x04:      // BARC table
            case 0x06:      // unknown table (small)
            case 0x07:      // unknown table (small)
                offset += 0x04; // offset
                offset += 0x08; // size + flags
                break;
            case 0x05:      // stream table (list of .afc and their config, useless since .afc also have them)
                offset += 0x04; // offset
                if (!hdr.big_endian)
                    offset += 0x04; // empty
                offset += 0x08; // size + flags
                break;

            case 0x02:      // IBNK offsets
            case 0x03: {    // WSYS offsets
                while (offset < max_offset) {
                    uint32_t entry_offset = read_u32(offset, sf);
                    offset += 0x04;
                    if (entry_offset == 0) // signals end of offsets
                        break;
                    if (!hdr.big_endian)
                        offset += 0x04; // empty
                    offset += 0x08; // size + entry id (size is also in IBNK/WSYS)

                    if (chunk_id == 0x03) {
                        bool ok = parse_jaudio_wsys(&hdr, sf, entry_offset, JAUDIO_AAF);
                        if (!ok) {
                            VGM_LOG("JAUDIO: wrong entry at %x\n", entry_offset);
                            return NULL;
                        }
                    }
                }
                break;
            }

            default:
                VGM_LOG("JAUDIO: unknown chunk %i at %x\n", chunk_id, offset - 0x04);
                return NULL;
        }
    }

    return init_vgmstream_jaudio(sf, &hdr);
}

/* .bx - Nintendo JAudio (v2?) games [Pikmin (GC/Switch)] */
VGMSTREAM* init_vgmstream_jaudio_bx(STREAMFILE* sf) {

    /* checks */
    if ((read_u32be(0x00, sf) != 0x10 || read_u32be(0x04, sf) > 0x100) &&
        (read_u32le(0x00, sf) != 0x10 || read_u32le(0x04, sf) > 0x100)) // common values
        return NULL;
    if (!check_extensions(sf, "bx"))
        return NULL;


    meta_header_t hdr = {0};
    meta_load_subsong(&hdr, sf);
    hdr.big_endian = guess_endian32(0x00, sf); // Pikmin 1 Switch uses LE

    read_u32_t read_u32 = hdr.big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hdr.big_endian ? read_s32be : read_s32le;

    uint32_t wsys_tables = read_u32(0x00, sf);
    int wsys_count = read_s32(0x04, sf);
    // 08: IBNK tables
    // 0c: IBNK count

    uint32_t offset = wsys_tables;
    for (int i = 0; i < wsys_count; i++) {
        uint32_t entry_offset   = read_u32(offset + 0x00, sf);
        uint32_t entry_size     = read_u32(offset + 0x04, sf);
        offset += 0x08;

        // often entries appear with only offset + no size, or no offset + no size 
        if (entry_offset == 0x00 || entry_size == 0x00)
            continue;

        bool ok = parse_jaudio_wsys(&hdr, sf, entry_offset, JAUDIO_BX);
        if (!ok) {
            VGM_LOG("JAUDIO: wrong entry at %x\n", entry_offset);
            return NULL;
        }
    }

    return init_vgmstream_jaudio(sf, &hdr);
}


static bool parse_jaudio_baa(STREAMFILE* sf, meta_header_t* hdr, uint32_t base_offset, uint32_t max_offset) {
    enum baa_chunks_t { 
        bst_ = 0x62737420, bstn = 0x6273746E, bsc_ = 0x62736320, ws__ = 0x77732020, bnk_ = 0x626E6B20, bfst = 0x62736674, bms_ = 0x626D7320, baac = 0x62616163, bfca = 0x62666361,
    };

    uint32_t offset = base_offset;
    if (!is_id32be(offset, sf, "AA_<"))
        return false;
    offset += 0x04;

    // parse chunks: N chunk ids each with offset + size
    while (offset < max_offset) {
        uint32_t chunk_id = read_u32be(offset, sf);
        offset += 0x04;

        if (chunk_id == get_id32be(">_AA")) // end chunk
            break;

        switch(chunk_id) {
            case bst_:
            case bstn:
            case bsc_:
            case bnk_:
                offset += 0x08; // entry offset + size/end offset
                break;
            case bfst:
            case bfca:
                offset += 0x04; // entry offset
                break;
            case bms_:
                offset += 0x0c; // flags/id + entry offset + end offset
                break;

            case ws__: {
                // 00: id
                uint32_t entry_offset = read_u32be(offset + 0x04, sf) + base_offset;
                // 08: flags? (0/-1)
                offset += 0x0c;

                bool ok = parse_jaudio_wsys(hdr, sf, entry_offset, JAUDIO_BAA);
                if (!ok) {
                    VGM_LOG("JAUDIO: wrong entry at %x\n", entry_offset);
                    return false;
                }

                break;
            }

            case baac: { // table with multiple 'baa' headers, each containing more ws [Mario Kart: Double Dash!! (GC)]
                uint32_t baac_offset = read_u32be(offset + 0x00, sf) + base_offset;
                uint32_t baac_end    = read_u32be(offset + 0x04, sf) + base_offset;
                offset += 0x08;

                uint32_t sub_offset = baac_offset;
                int entries = read_s32be(sub_offset + 0x00, sf);
                sub_offset += 0x04;
                for (int i = 0; i < entries; i++) {
                    uint32_t baa_offset = read_u32be(sub_offset, sf) + baac_offset;
                    sub_offset += 0x04;

                    bool ok = parse_jaudio_baa(sf, hdr, baa_offset, baac_end);
                    if (!ok) return false;
                }

                break;
            }


            default:
                VGM_LOG("JAUDIO: unknown chunk %x at %x\n", chunk_id, offset - 0x04);
                return false;
        }
    }

    return true;
}

/* .baa - Nintendo JAudio (v3?) games [Mario Kart - Double Dash!! (GC), Super Mario Galaxy (Wii), Zelda: Twilight Princess (Wii)] */
VGMSTREAM* init_vgmstream_jaudio_baa(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "AA_<"))
        return NULL;
    if (!check_extensions(sf, "baa"))
        return NULL;


    meta_header_t hdr = {0};
    meta_load_subsong(&hdr, sf);
    hdr.big_endian = true;

    uint32_t max_offset = get_streamfile_size(sf);
    uint32_t offset = 0x00;

    bool ok = parse_jaudio_baa(sf, &hdr, offset, max_offset);
    if (!ok) return NULL;

    return init_vgmstream_jaudio(sf, &hdr);
}
