#include "meta.h"
#include "../coding/coding.h"

#define PS2P_COUNT_OFFSET       0x14
#define PS2P_AUX_COUNT_OFFSET   0x18
#define PS2P_TABLE_OFFSET       0x20
#define PS2P_ENTRY_SIZE         0x0C
#define PS2P_AUX_ENTRY_SIZE     0x1C

/* THQ Australia (Studio Oz) - PS2P [Jimmy Neutron: Attack of the Twonkies (PS2), SpongeBob: Lights, Camera, Pants! (PS2)] */
VGMSTREAM* init_vgmstream_ps2p(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, file_count, aux_count, alignment;
    int target_subsong = sf->stream_index;

    if (!is_id32be(0x00, sf, "ps2p"))
        return NULL;

    if (!check_extensions(sf, "sounds"))
        return NULL;

    /* Header Values */
    alignment = read_u32le(0x0C, sf);
    file_count = read_u32le(PS2P_COUNT_OFFSET, sf);
    aux_count = read_u32le(PS2P_AUX_COUNT_OFFSET, sf);

    if (file_count < 1) return NULL;

    /* Total subsongs equals file_count. */
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > (int)file_count) return NULL;

    /*
     * FORMAT LOGIC (Rotated Table):
     * 1. File 0 (Subsong 1): Offset = alignment, Size = Table1[0].size
     * 2. File i (Subsong i+1): Offset = Table1[i-1].offset, Size = Table1[i].size
     */

    if (target_subsong == 1) {
        subfile_offset = alignment;
        subfile_size = read_u32le(PS2P_TABLE_OFFSET, sf);
    }
    else {
        int prev_entry_idx = target_subsong - 2;
        int curr_entry_idx = target_subsong - 1;

        subfile_offset = read_u32le(PS2P_TABLE_OFFSET + (prev_entry_idx * PS2P_ENTRY_SIZE) + 0x08, sf);
        subfile_size = read_u32le(PS2P_TABLE_OFFSET + (curr_entry_idx * PS2P_ENTRY_SIZE) + 0x00, sf);
    }

    if (subfile_offset == 0) return NULL;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "vag");
    if (!temp_sf) return NULL;

    vgmstream = init_vgmstream_vag(temp_sf);

    if (vgmstream) {
        vgmstream->num_streams = file_count;
        vgmstream->meta_type = meta_PS2P;

        /* Table 2 (Aux/Mapping) maps Strings to File IDs.
         * Structure: [FileID (4)] [Unk (24)].
         * If mapping found for our current File ID, look up the string. */
        uint32_t table1_size = file_count * PS2P_ENTRY_SIZE;
        uint32_t table2_offset = PS2P_TABLE_OFFSET + table1_size;
        uint32_t table2_size = aux_count * PS2P_AUX_ENTRY_SIZE;

        /* String table starts 4 bytes before the calculated end of Table 2. */
        uint32_t string_table_offset = table2_offset + table2_size - 4;

        int current_file_id = target_subsong - 1;
        int mapping_idx = -1;

        /* Scan Mapping Table to find the String Index for this File ID */
        for (int i = 0; i < (int)aux_count; i++) {
            uint32_t mapped_id = read_u32le(table2_offset + (i * PS2P_AUX_ENTRY_SIZE), sf);
            if (mapped_id == (uint32_t)current_file_id) {
                mapping_idx = i;
                break;
            }
        }

        /* If a mapping exists, overwrite the VAG internal name with the Mapped Name */
        if (mapping_idx >= 0) {
            off_t name_offset = string_table_offset;
            int strings_to_skip = mapping_idx;

            while (strings_to_skip > 0 && name_offset < (off_t)alignment) {
                char c = read_u8(name_offset, sf);
                if (c == '\0') {
                    strings_to_skip--;
                }
                name_offset++;
            }

            if (name_offset < (off_t)alignment) {
                read_string(vgmstream->stream_name, STREAM_NAME_SIZE, name_offset, sf);
            }
        }
    }

    close_streamfile(temp_sf);
    return vgmstream;
}