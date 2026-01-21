#include "meta.h"
#include "../coding/coding.h"

#define THQ_AUS_PS2P_ALIGNMENT_OFFSET   0x0C
#define THQ_AUS_PS2P_COUNT_OFFSET       0x14
#define THQ_AUS_PS2P_AUX_COUNT_OFFSET   0x18
#define THQ_AUS_PS2P_TABLE_OFFSET       0x20
#define THQ_AUS_PS2P_ENTRY_SIZE         0x0C
#define THQ_AUS_PS2P_AUX_ENTRY_SIZE     0x1C

/* THQ Australia - PS2P [Jimmy Neutron: Attack of the Twonkies (PS2), SpongeBob: Lights, Camera, Pants! (PS2)] */
VGMSTREAM* init_vgmstream_thq_aus_ps2p(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, file_count, aux_count;
    int target_subsong = sf->stream_index;

    if (!check_extensions(sf, "sounds"))
        return NULL;

    if (!is_id32be(0x00, sf, "ps2p"))
        return NULL;

    /* Header Values */
    file_count = read_u32le(THQ_AUS_PS2P_COUNT_OFFSET, sf);
    aux_count = read_u32le(THQ_AUS_PS2P_AUX_COUNT_OFFSET, sf);

    if (file_count < 1) return NULL;

    /* Total subsongs equals file_count. */
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > file_count) return NULL;

    /*
     * FORMAT LOGIC:
     * 1. IMPLICIT FILE 0: The first file (Subsong 1) is NOT in the File Table offset list.
     *    It starts at the Alignment Offset.
     * 2. ROTATED TABLE: The Table starts describing offsets for File 1 onwards.
     *    Entry[i].Size   = Size of File[i]
     *    Entry[i].Offset = Offset of File[i+1]
     */

    if (target_subsong == 1) {
        subfile_offset = read_u32le(THQ_AUS_PS2P_ALIGNMENT_OFFSET, sf);
        subfile_size = read_u32le(THQ_AUS_PS2P_TABLE_OFFSET, sf);
    }
    else {
        /* Files 1..N */
        /* To get File i (target_subsong), look at Table Entry i-1 for Offset,
         * and Table Entry i for Size. */
        int entry_idx = target_subsong - 2; // Map Subsong 2 -> Entry 0

        /* Offset is at 0x08 in the previous entry */
        subfile_offset = read_u32le(THQ_AUS_PS2P_TABLE_OFFSET + (entry_idx * THQ_AUS_PS2P_ENTRY_SIZE) + 0x08, sf);

        /* Size is at 0x00 in the current entry */
        subfile_size = read_u32le(THQ_AUS_PS2P_TABLE_OFFSET + ((entry_idx + 1) * THQ_AUS_PS2P_ENTRY_SIZE), sf);
    }

    if (subfile_offset == 0) return NULL;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "vag");
    if (!temp_sf) return NULL;

    vgmstream = init_vgmstream_vag(temp_sf);

    if (vgmstream) {
        vgmstream->num_streams = file_count;
        vgmstream->meta_type = meta_THQ_AUS_PS2P;

        /* --- NAME MAPPING --- */
        /* Table 2 (Aux/Mapping) maps Strings to File IDs.
         * Structure: [FileID (4)] [Unk (24)].
         * If mapping found for our current File ID, look up the string. */

        uint32_t table1_size = file_count * THQ_AUS_PS2P_ENTRY_SIZE;
        uint32_t table2_offset = THQ_AUS_PS2P_TABLE_OFFSET + table1_size;
        uint32_t table2_size = aux_count * THQ_AUS_PS2P_AUX_ENTRY_SIZE;

        /* String table starts 4 bytes before the calculated end of Table 2. */
        uint32_t string_table_offset = table2_offset + table2_size - 4;

        /* Current File ID (0-based) */
        int current_file_id = target_subsong - 1;
        int mapping_idx = -1;

        /* Scan Mapping Table to find which String Index maps to this File ID */
        for (int i = 0; i < aux_count; i++) {
            uint32_t mapped_id = read_u32le(table2_offset + (i * THQ_AUS_PS2P_AUX_ENTRY_SIZE), sf);
            if (mapped_id == current_file_id) {
                mapping_idx = i;
                break;
            }
        }

        /* If mapped, fetch the string */
        if (mapping_idx >= 0) {
            off_t name_offset = string_table_offset;
            int i;

            /* Scan through packed null-terminated strings to reach mapping_idx */
            for (i = 0; i < mapping_idx; i++) {
                char c;
                /* Skip string */
                do {
                    if (read_streamfile((uint8_t*)&c, name_offset, 1, sf) != 1) break;
                    name_offset++;
                } while (c != '\0');
            }
            read_string(vgmstream->stream_name, STREAM_NAME_SIZE, name_offset, sf); //Use name table then VAGp if mapped does not exist.
        }
    }

    close_streamfile(temp_sf);
    return vgmstream;
}