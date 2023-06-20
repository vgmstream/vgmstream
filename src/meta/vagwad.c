#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* VAGWAD - Jak & Daxter: The Precursor Legacy, Jak II, Jak 3, Jak X (PS2) */
VGMSTREAM* init_vgmstream_vagwad(STREAMFILE* sf_wad) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_dir = NULL;
    bool is_int_wad, is_jak_ii;
    char header_name[17], name_char, sound_name[9];
    int channels = 0, entries, interleave = 0, sample_rate = 0, stereo = -1, total_subsongs = 0, target_subsong = sf_wad->stream_index, version;
    int i, loop_flag, meta_type, name_idx, name_int;
    off_t entry_offset, stream_offset = 0;
    read_u32_t read_u32;
    size_t entry_size, comp_name_size = 38, header_size = 0x30, stream_size;
    uint64_t compressed_name, entry;

    /* checks */
    if (!is_id32be(0x00, sf_wad, "VAGp") && !is_id32be(0x00, sf_wad, "pGAV"))
        goto fail;

    if (!check_extensions(sf_wad, "com,eng,fre,ger,int,ita,jap,kor,spa"))
        goto fail;

    sf_dir = open_streamfile_by_filename(sf_wad, "VAGDIR.AYB");
    if (!sf_dir)
        goto fail;


    if (!target_subsong)
        target_subsong = 1;

    if (is_id64be(0x00, sf_dir, "VGWADDIR")) { /* Jak 3, Jak X */
        is_int_wad = check_extensions(sf_wad, "int");
        entry_size = 0x08; /* compressed entries */
        entry_offset = 0x10;

        version = read_u32le(0x08, sf_dir);
        entries = read_u32le(0x0C, sf_dir); /* Not total_subsongs, VAGWAD.INT entries are separate */

        //if (version < 2 || version > 3) /* v.2 = Jak 3, v.3 = Jak X */
        //    goto fail;
        switch (version) {
            case 2: /* Jak 3 */
                interleave = 0x2000;
                break;
            case 3: /* Jak X */
                interleave = 0x1000;
                break;
            default:
                goto fail;
        }

        for (i = 0; i < entries; i++) {
            entry = read_u64le(entry_offset, sf_dir);

            if ((bool)(entry >> 43 & 0x1) == is_int_wad) { /* VAGWAD.INT bool */
                total_subsongs++;

                if (total_subsongs == target_subsong) { /* is at the correct target song index */
                    compressed_name = entry & 0x3FFFFFFFFFF; /* 42-bit compressed filename */
                    stereo = entry >> 42 & 0x1;
                    if (!stereo)
                        interleave = 0;
                    stream_offset = entry >> 48 << 15; /* aligned to 0x8000 */

                    name_int = compressed_name & 0x1FFFFF; /* split in two 21-bit fields */
                    for (name_idx = 7; name_idx >= 0; name_idx--) {
                        if (name_idx == 3)
                            name_int = compressed_name >> 21;
                        name_char = name_int % comp_name_size;
                        name_int /= comp_name_size;

                        if (!name_char) /* empty space */
                            name_char = 0x20;
                        else if (name_char <= 26) /* A-Z */
                            name_char += 0x40;
                        else if (name_char <= 36) /* 0-9 */
                            name_char += 0x15;
                        else /* any other char, hyphen */
                            name_char = 0x2D;

                        sound_name[name_idx] = name_char;
                    }
                    sound_name[8] = 0x00;
                }
            }
            entry_offset += entry_size;
        }
    }
    else if (read_u32le(0x0C, sf_dir) == 0x00) { /* J&D: TPL, Jak II */
        //is_jak_ii = is_id32be(0x00, sf_wad, "pGAV"); /* VAGp in J&D: TPL */
        is_jak_ii = read_u32le(0x10, sf_dir) < 2; /* stereo bool, not in J&D: TPL */
        entry_size = is_jak_ii ? 0x10 : 0x0C;
        entry_offset = 0x04;

        total_subsongs = read_u32le(0x00, sf_dir);

        for (i = 1; i <= total_subsongs; i++) {
            if (i == target_subsong) {
                read_string(sound_name, 9, entry_offset, sf_dir);
                stream_offset = read_u32le(entry_offset + 0x08, sf_dir) << 11; /* aligned to 0x800 */
                stereo = is_jak_ii ? read_u32le(entry_offset + 0x0C, sf_dir) : 0;
                if (stereo > 1)
                    goto fail;
                interleave = stereo ? 0x2000 : 0;
            }
            entry_offset += entry_size;
        }
    }
    else
        goto fail;

    if (total_subsongs < 1 || target_subsong > total_subsongs)
        goto fail;

    if (is_id32be(stream_offset, sf_wad, "pGAV") && is_id32be(stream_offset + interleave, sf_wad, "pGAV")) {
        read_u32 = read_u32le;
        meta_type = meta_PS2_pGAV;
    }
    else if (is_id32be(stream_offset, sf_wad, "VAGp")) {
        read_u32 = read_u32be;
        meta_type = meta_PS2_VAGp;
    }
    else
        goto fail;

    loop_flag = 0;
    channels = stereo + 1; /* stereo bool to channels */

    stream_size = read_u32(stream_offset + 0x0C, sf_wad);
    sample_rate = read_u32(stream_offset + 0x10, sf_wad);

    //read_string(header_name, 17, stream_offset + 0x20, sf_wad);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream)
        goto fail;

    vgmstream->meta_type = meta_type;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = stereo ? layout_interleave : layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;
    vgmstream->interleave_block_size = interleave;
    vgmstream->interleave_first_skip = header_size;
    vgmstream->interleave_first_block_size = interleave - header_size; /* interleave includes header */
    vgmstream->num_samples = ps_bytes_to_samples(stream_size / channels, 1);

    /* only J&D: TPL has unique names, otherwise just "Mono" and "Stereo" */
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", sound_name);
    //snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", sound_name, header_name);

    if (!vgmstream_open_stream(vgmstream, sf_wad, stream_offset + header_size))
        goto fail;

    close_streamfile(sf_dir);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sf_dir);
    return NULL;
}
