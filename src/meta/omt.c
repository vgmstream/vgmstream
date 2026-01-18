#include "meta.h"
#include "../coding/coding.h"

#define OMT_WAVE_SCAN_WINDOW 0x40
#define OMT_ENTRY_FIXED_SIZE 13

/* OMT/0MF2 - AWE Productions - SpongeBob SquarePants: Employee of the Month*/

VGMSTREAM* init_vgmstream_0MF2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t dir_offset, entry_offset, data_offset = 0;
    size_t data_size = 0;
    int total_subsongs = 0;
    int target_subsong = sf->stream_index;
    int i;
    char stream_name[STREAM_NAME_SIZE] = {0};
    int wave_found = 0;

    if (!check_extensions(sf, "omt"))
        return NULL;

    if (!is_id32be(0x00, sf, "0MF2"))
        return NULL;

    dir_offset = read_u32be(0x04, sf);

    for (i = 0; i < OMT_WAVE_SCAN_WINDOW; i++) {
        if (is_id32be(dir_offset + i, sf, "Wave")) {
            dir_offset += i;
            wave_found = 1;
            break;
        }
    }

    if (!wave_found) {
        return NULL;
    }

    /* "Wave" (4 bytes) + Count (4 bytes) */
    total_subsongs = read_u32be(dir_offset + 0x04, sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        return NULL;

    entry_offset = dir_offset + 0x08;

    for (i = 0; i < total_subsongs; i++) {
        uint32_t entry_off_val, entry_size_val;
        size_t name_len;

        /* Entry Layout (Big Endian):
           0x00: ID (4 bytes)
           0x04: Offset (4 bytes)
           0x08: Size (4 bytes)
           0x0C: Name Length (1 byte)
           0x0D: Name (Pascal String) (Name Length bytes)
        */

        entry_off_val = read_u32be(entry_offset + 0x04, sf);
        entry_size_val = read_u32be(entry_offset + 0x08, sf);
        name_len = read_u8(entry_offset + 0x0C, sf);

        if (i + 1 == target_subsong) {
            data_offset = entry_off_val;
            data_size = entry_size_val;
            if (name_len > 0) {
                read_streamfile((uint8_t*)stream_name, entry_offset + 0x0D, name_len, sf);
            }
            break;
        }

        /* Fixed headers (13 bytes) + name */
        entry_offset += OMT_ENTRY_FIXED_SIZE + name_len;
    }

    if (data_offset == 0 || data_size == 0)
        return NULL;

    temp_sf = setup_subfile_streamfile(sf, data_offset, data_size, "wav");
    if (!temp_sf) return NULL;

    vgmstream = init_vgmstream_riff(temp_sf);

    close_streamfile(temp_sf);

    if (!vgmstream) return NULL;

    vgmstream->num_streams = total_subsongs;
    vgmstream->meta_type = meta_0MF2;

    if (stream_name[0] != '\0') {
        strcpy(vgmstream->stream_name, stream_name);
    }

    return vgmstream;
}