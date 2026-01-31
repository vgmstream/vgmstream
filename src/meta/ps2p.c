#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

#define PS2P_TABLE_OFFSET       0x20
#define PS2P_ENTRY_SIZE         0x0C
#define PS2P_AUX_ENTRY_SIZE     0x1C
#define PS2P_AUX_OFF_END_PTR    0x18

static int ps2p_is_id_mapped(STREAMFILE* sf, uint32_t aux_count, uint32_t aux_table_offset, uint32_t target_id) {
    for (uint32_t i = 0; i < aux_count; i++) {
        uint32_t entry_addr = aux_table_offset + (i * PS2P_AUX_ENTRY_SIZE);
        uint32_t id = read_u32le(entry_addr, sf); /* First 4 bytes is ID */
        if (id == target_id) return 1;
    }
    return 0;
}

/* Calculate absolute offset and size for a specific File ID using the rotated table */
static int ps2p_get_file_info(STREAMFILE* sf, uint32_t file_id, uint32_t alignment, uint32_t table_offset, uint32_t* offset, uint32_t* size) {
    if (file_id == 0) {
        *offset = alignment;
        *size = read_u32le(table_offset, sf);
    } else {
        /* Offset is at 0x08 of previous entry */
        *offset = read_u32le(table_offset + ((file_id - 1) * PS2P_ENTRY_SIZE) + 0x08, sf);
        /* Size is at 0x00 of current entry */
        *size = read_u32le(table_offset + (file_id * PS2P_ENTRY_SIZE) + 0x00, sf);
    }
    return (*offset != 0 && *size != 0);
}

/* THQ Australia (Studio Oz) - PS2P [Jimmy Neutron: Attack of the Twonkies (PS2), SpongeBob: Lights, Camera, Pants! (PS2)] */
VGMSTREAM* init_vgmstream_ps2p(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    VGMSTREAM* vgm_L = NULL;
    VGMSTREAM* vgm_R = NULL;
    STREAMFILE* temp_sf = NULL;

    uint32_t file_count, aux_count, alignment;
    uint32_t table1_size, table2_offset;
    uint32_t file_id_L, file_id_R = 0;
    int channels = 1;

    if (!is_id32be(0x00, sf, "ps2p"))
        return NULL;
    if (!check_extensions(sf, "sounds"))
        return NULL;

    alignment = read_u32le(0x0C, sf);
    file_count = read_u32le(0x14, sf);
    aux_count = read_u32le(0x18, sf);

    if (file_count < 1) return NULL;

    int total_subsongs = (aux_count > 0) ? aux_count : file_count;
    int target_subsong = sf->stream_index;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs) return NULL;

    table1_size = file_count * PS2P_ENTRY_SIZE;
    table2_offset = PS2P_TABLE_OFFSET + table1_size;

    /* Stereo Check */
    if (aux_count > 0) {
        /* Map Subsong Index -> File ID via Aux Table */
        uint32_t aux_entry_addr = table2_offset + ((target_subsong - 1) * PS2P_AUX_ENTRY_SIZE);
        file_id_L = read_u32le(aux_entry_addr, sf);

        /* Check for Implicit Stereo (Orphan) */
        /* If the NEXT file ID exists but is NOT in the Aux table, it's the Right Channel */
        uint32_t next_id = file_id_L + 1;
        if (next_id < file_count) {
            if (!ps2p_is_id_mapped(sf, aux_count, table2_offset, next_id)) {
                channels = 2;
                file_id_R = next_id;
            }
        }
    } else {
        /* Fallback: 1:1 mapping (Mono only) */
        file_id_L = target_subsong - 1;
    }

    {
        uint32_t off_L, size_L;
        if (!ps2p_get_file_info(sf, file_id_L, alignment, PS2P_TABLE_OFFSET, &off_L, &size_L))
            goto fail;

        temp_sf = setup_subfile_streamfile(sf, off_L, size_L, "vag");
        if (!temp_sf) goto fail;

        vgm_L = init_vgmstream_vag(temp_sf);

        if (!vgm_L) {
            close_streamfile(temp_sf);
            goto fail;
        }
    }

    if (channels == 1) {
        vgmstream = vgm_L;
        vgmstream->meta_type = meta_PS2P;
        vgmstream->num_streams = total_subsongs;
    }
    else {
        uint32_t off_R, size_R;
        if (!ps2p_get_file_info(sf, file_id_R, alignment, PS2P_TABLE_OFFSET, &off_R, &size_R))
            goto fail;

        temp_sf = setup_subfile_streamfile(sf, off_R, size_R, "vag");
        if (!temp_sf) goto fail;

        vgm_R = init_vgmstream_vag(temp_sf);
        if (!vgm_R) {
            close_streamfile(temp_sf);
            goto fail;
        }

        vgmstream = allocate_vgmstream(channels, 0);
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_PS2P;
        vgmstream->layout_type = layout_layered;
        vgmstream->num_streams = total_subsongs;

        vgmstream->layout_data = init_layout_layered(channels);
        if (!vgmstream->layout_data) goto fail;

        layered_layout_data* data = vgmstream->layout_data;
        data->layers[0] = vgm_L;
        data->layers[1] = vgm_R;

        if (!setup_layout_layered(data))
            goto fail;

        vgmstream->sample_rate = vgm_L->sample_rate;
        vgmstream->num_samples = vgm_L->num_samples; // Usually min(L,R)
        vgmstream->coding_type = vgm_L->coding_type;

        vgm_L = NULL;
        vgm_R = NULL;
    }

    /* Parse Name Table */
    if (aux_count > 0) {

        /* The String Table ALWAYS starts at Offset 0x18 of the Last Aux Entry.
           This is 4 bytes before the end of the table. */
        uint32_t table2_size = aux_count * PS2P_AUX_ENTRY_SIZE;
        uint32_t string_table_start = table2_offset + table2_size - 0x04;

        uint32_t name_rel_offset = 0;
        int idx = target_subsong - 1; /* Maps directly to the Aux Index */

        if (idx > 0) {
            /* The Start Offset for string N is the End Offset of string N-1 */
            uint32_t prev_name_ent = table2_offset + ((idx - 1) * PS2P_AUX_ENTRY_SIZE);
            name_rel_offset = read_u32le(prev_name_ent + PS2P_AUX_OFF_END_PTR, sf);
        }

        if ((string_table_start + name_rel_offset) < get_streamfile_size(sf)) {
            read_string(vgmstream->stream_name, STREAM_NAME_SIZE, string_table_start + name_rel_offset, sf);
        }
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    else {
        if (vgm_L) close_vgmstream(vgm_L);
        if (vgm_R) close_vgmstream(vgm_R);
    }
    return NULL;
}
