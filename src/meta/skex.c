#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"


/* SKEX - from SCE America second party devs [Syphon Filter: Dark Mirror (PS2/PSP), MLB 2004 (PS2), MLB 15 (Vita)] */
VGMSTREAM* init_vgmstream_skex(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* sf_h = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "SKEX"))
        return NULL;
    if (!check_extensions(sf,"skx"))
        return NULL;

    // bank-like format with helper files typically found inside memory/bigfiles (.SWD, .DAT, etc)
    // - .skx: external streams (pack of full formats)
    // - .tbl: main stream info
    // - .ctl: cues?
    // - .mrk: text script related to .tbl
    // usually .tbl is the header and .skx its body, but rarely may be combined so use .skx as a base

    uint16_t version         = read_u16le(0x04, sf); // in hex NN.NN form
    // 06: low number, seems related to file (id?)
    // 08: null
    // 0c: null
    uint32_t head_offset    = read_u32le(0x10, sf);
    uint32_t head_size      = read_u32le(0x14, sf);
    int entries             = read_u16le(0x18, sf); // even with no head_offset/size

    // micro optimization (empty banks do exist)
    if (get_streamfile_size(sf) <= 0x100) {
        vgm_logi("SKEX: bank has no subsongs (ignore)\n");
        return NULL;
    }

    // setup header
    if (head_offset && head_size) {
        // rare [MLB 2004 (PS2), NBA 06 (PS2)]
        sf_h = sf;
    }
    else {
        // note that may .skx may be uppercase and companion file lowercase (meaning Linux won't open this)
        sf_h = open_streamfile_by_ext(sf, "tbl");
        if (!sf_h) {
            vgm_logi("SKEX: companion file .tbl not found (put together)\n");
            return NULL;
        }
    }


    uint32_t subfile_offset = 0, subfile_size = 0, prev_offset = 0, subfile_type = 0;

    int total_subsongs = 0;
    int target_subsong = sf->stream_index;
    if (target_subsong == 0) target_subsong = 1;

    // Entries have many repeats so calculate totals.
    // After last entry there is a fake entry with .skx size (meaning next_offset is always valid).
    // With flags = 0x1000, after all is another table with increasing low number per entry
    switch(version) {
        case 0x1070: {  // MLB 2003 (PS2), MLB 2004 (PS2)
            uint32_t offset = head_offset;

            // entries go after files
            for (int i = 0; i < entries; i++) {
                uint32_t curr_offset = read_u32le(offset + 0x00, sf_h);
                uint32_t curr_type   = read_u32le(offset + 0x04, sf_h);
                // 08: null?

                offset += 0x0c;

                switch(curr_type) {
                    case 0x05: // .vag (mono)
                    case 0x0c: // .vag (stereo)
                        break;
                    default:
                        vgm_logi("SKEX: unknown format %x (report)\n", curr_type);
                        goto fail;
                }

                if (prev_offset == curr_offset)
                    continue;
                prev_offset = curr_offset;

                total_subsongs++;

                if (target_subsong == total_subsongs && !subfile_offset) {
                    uint32_t next_offset = read_u32le(offset, sf_h);
                    subfile_offset = curr_offset;
                    subfile_size = next_offset - curr_offset;
                    subfile_type = curr_type;
                }
            }
            break;
        }

        case 0x2040:    // MLB 2005 (PS2)
        case 0x2070:    // MLB 2006 (PS2), NBA 06 (PS2), MLB (PSP)
        case 0x3000: {  // Syphon Filter: Dark Mirror (PS2/PSP), Syphon Filter: Logan's Shadow (PSP)
            uint32_t offset = head_offset;

            // 00: header id
            // 04: version
            // 06: low number, seems related to file
            // 08: entries (same as .skx)
            // 0a: flags
            // 0c: null?
            // 10: entries again?
            if (!is_id32be(offset + 0x00,sf_h, "STBL")) {
                VGM_LOG("SKEX: incorrect .tbl\n");
                goto fail;
            }
            offset += 0x50;

            for (int i = 0; i < entries; i++) {
                uint32_t curr_offset = read_u32le(offset + 0x00, sf_h);
                // 04: 0 or 1 (doesn't seem to be related to loops, companion files or such)
                // 05: null?
                // 06: null?
                uint8_t  curr_type   = read_u8   (offset + 0x07, sf_h);

                offset += 0x08;

                switch(curr_type) {
                    case 0x00: // dummy/config?
                    case 0x01: // dummy/config?
                    case 0x0e: // "Names" in .skx (empty?)
                        continue;
                    case 0x05: // .vag (mono)
                    case 0x09: // .at3
                    case 0x0b: // .vpk
                    case 0x0c: // .vag (stereo)
                        break;
                    default:
                        vgm_logi("SKEX: unknown format %x (report)\n", curr_type);
                        goto fail;
                }

                if (prev_offset == curr_offset)
                    continue;
                prev_offset = curr_offset;

                total_subsongs++;

                if (target_subsong == total_subsongs && !subfile_offset) {
                    uint32_t next_offset = read_u32le(offset, sf_h);
                    subfile_offset = curr_offset;
                    subfile_size = next_offset - curr_offset;
                    subfile_type = curr_type;
                }
            }
            break;
        }

        case 0x5100: {   // MLB 14 (Vita), MLB 15 (Vita)
            uint32_t offset = head_offset;
            uint16_t multiplier, align = 0;

            // 00: header id
            // 04: version
            // 06: low number, seems related to file
            // 08: entries (same as .skx)
            // 0a: null?
            // 0c: file size (without padding)
            // 10: offset to 2nd table
            // 14: null?
            // 18: offset multiplier (0x800/0x400/0x01)
            // 1a: flags? (rarely 0x08)
            // 1c: some entries?
            // 20: null
            // 24: entries again?
            if (!is_id32be(offset + 0x00,sf_h, "STBL")) {
                VGM_LOG("SKEX: incorrect .tbl\n");
                goto fail;
            }
            multiplier = read_u16le(offset + 0x18, sf_h);

            offset += 0x64;
            for (int i = 0; i < entries; i++) {
                uint32_t curr_offset = read_u32le(offset + 0x00, sf_h) * multiplier;
                // 04: null?
                uint8_t  curr_type   = read_u8   (offset + 0x05, sf_h);

                offset += 0x06;

                switch(curr_type) {
                    case 0x00: // dummy?
                    case 0x01: // some config?
                    case 0x0e: // "<HR_EMITTER>"
                    case 0x0f: // MIDX (maybe some instrument/midi definition, but data doesn't look midi-like)
                        continue;
                    case 0x02: // .at9
                    case 0x42: // .at9 (no diffs?)
                        break;
                    default:
                        vgm_logi("SKEX: unknown format %x (report)\n", curr_type);
                        goto fail;
                }

                // oddly misaligned by 1, no apparent flags [MLB 15 (Vita)-FEPXP.SKX]
                if (curr_offset == 0x00 && multiplier == 0x800 && !align) {
                    align = multiplier;
                }
                curr_offset += align;

                if (prev_offset == curr_offset)
                    continue;
                prev_offset = curr_offset;

                total_subsongs++;

                if (target_subsong == total_subsongs && !subfile_offset) {
                    uint32_t next_offset = read_u32le(offset, sf_h) * multiplier + align;
                    subfile_offset = curr_offset;
                    subfile_size = next_offset - curr_offset;
                    subfile_type = curr_type;
                }
            }
            break;
        }
        default:
            goto fail;
    }

    if (total_subsongs == 0) {
        vgm_logi("SKEX: bank has no subsongs (ignore)\n"); //sometimes
        goto fail;
    }

    if (!check_subsongs(&target_subsong, total_subsongs))
        goto fail;

    ;VGM_LOG("subfile=%x, %x, %x, %i\n", subfile_offset, subfile_size, subfile_type, total_subsongs);
    {
        init_vgmstream_t init_vgmstream = NULL;
        const char* ext;
        switch(subfile_type) {
            case 0x05:
            case 0x0c:  init_vgmstream = init_vgmstream_vag; ext = "vag"; break;
            case 0x09:  init_vgmstream = init_vgmstream_riff; ext = "at3"; break;
            case 0x0b:  init_vgmstream = init_vgmstream_vpk; ext = "vpk"; break;
            case 0x02:
            case 0x42:  init_vgmstream = init_vgmstream_riff; ext = "at9"; break;
            default: goto fail;
        }

        if (subfile_type == 0x09 || subfile_type == 0x02) { // use RIFF's
            subfile_size = read_u32le(subfile_offset + 0x04, sf) + 0x08;
        }

        temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, ext);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream(temp_sf);
        if (!vgmstream) goto fail;
    }

    vgmstream->num_streams = total_subsongs;

    if (sf_h != sf) close_streamfile(sf_h);
    close_streamfile(temp_sf);
    return vgmstream;
fail:
    if (sf_h != sf) close_streamfile(sf_h);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
