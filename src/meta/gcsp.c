#include "meta.h"
#include "../coding/coding.h"

/* Global Header Offsets */
#define GCSP_OFF_DATA_START     0x0C
#define GCSP_OFF_FILE_COUNT     0x14
#define GCSP_OFF_NAME_COUNT     0x18
#define GCSP_OFF_TABLE_START    0x20

/* File Entry Structure (Size: 0x44) */
#define GCSP_ENTRY_SIZE         0x44
#define GCSP_ENT_NIBBLES        0x0C
#define GCSP_ENT_COEFS          0x10
#define GCSP_ENT_HIST           0x34
#define GCSP_ENT_NEXT_OFFSET    0x40

/* Name Table Structure (Stride: 0x1C) */
#define GCSP_NAME_ENT_SIZE      0x1C
#define GCSP_NAME_OFF_ID        0x00
#define GCSP_NAME_OFF_END_PTR   0x18

/* Check if a specific File ID appears in the Name Table */
static int is_file_id_named(STREAMFILE* sf, uint32_t name_table_start, uint32_t name_count, uint32_t target_id) {
    int i;
    for (i = 0; i < name_count; i++) {
        uint32_t entry_addr = name_table_start + (i * GCSP_NAME_ENT_SIZE);
        uint32_t id = read_u32be(entry_addr + GCSP_NAME_OFF_ID, sf);
        if (id == target_id) return 1;
    }
    return 0;
}

/* Get the absolute start offset of a File ID  */
static uint32_t get_file_offset(STREAMFILE* sf, uint32_t file_id, uint32_t data_start) {
    if (file_id == 0) {
        return data_start;
    } else {
        /* The start of File N is stored at offset 0x40 of Entry N-1 */
        uint32_t prev_entry = GCSP_OFF_TABLE_START + ((file_id - 1) * GCSP_ENTRY_SIZE);
        return read_u32be(prev_entry + GCSP_ENT_NEXT_OFFSET, sf);
    }
}

/* THQ Australia (Studio Oz) - GCSP [SpongeBob: Lights, Camera, Pants! (GC), Jimmy Neutron: Attack of the Twonkies (GC)] */
VGMSTREAM* init_vgmstream_gcsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t data_start, file_count, aux_count;
    uint32_t file_id_L, file_id_R = 0;
    int channels = 1;
    int target_subsong = sf->stream_index;

    if (!is_id32be(0x00, sf, "gcsp"))
        return NULL;

    if (!check_extensions(sf, "sounds"))
        return NULL;

    data_start = read_u32be(GCSP_OFF_DATA_START, sf);
    file_count = read_u32be(GCSP_OFF_FILE_COUNT, sf);
    aux_count = read_u32be(GCSP_OFF_NAME_COUNT, sf);

    if (file_count < 1) return NULL;

    /* If names exist, the Name Table defines the Playable Streams.
       Orphan files (files without names) are treated as Right Channels of the preceding named file.
    */
    int total_streams = (aux_count > 0) ? aux_count : file_count;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_streams) return NULL;

    uint32_t name_table_start = GCSP_OFF_TABLE_START + (file_count * GCSP_ENTRY_SIZE);

    /* Stereo Check */
    if (aux_count > 0) {
        /* Get File ID from the Name Entry matching the target subsong */
        uint32_t name_ent_addr = name_table_start + ((target_subsong - 1) * GCSP_NAME_ENT_SIZE);
        file_id_L = read_u32be(name_ent_addr + GCSP_NAME_OFF_ID, sf);

        /* Check implicit stereo: Is Next ID (ID+1) an orphan? */
        uint32_t next_id = file_id_L + 1;
        if (next_id < file_count) {
             /* If next_id exists in file table but NOT in name table, it's the Right Channel */
             if (!is_file_id_named(sf, name_table_start, aux_count, next_id)) {
                 channels = 2;
                 file_id_R = next_id;
             }
        }
    } else {
        /* No name table: 1:1 mapping, mono only */
        file_id_L = target_subsong - 1;
    }

    uint32_t entry_L = GCSP_OFF_TABLE_START + (file_id_L * GCSP_ENTRY_SIZE);
    uint32_t num_samples = dsp_nibbles_to_samples(read_u32be(entry_L + GCSP_ENT_NIBBLES, sf));

    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) return NULL;

    vgmstream->sample_rate = 22050; /* Hardcoded (Not in header) */
    vgmstream->num_samples = num_samples;
    vgmstream->meta_type   = meta_GCSP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->num_streams = total_streams;

    /* Channel 0 (Left) */
    uint32_t offset_L = get_file_offset(sf, file_id_L, data_start);
    if (!vgmstream_open_stream(vgmstream, sf, offset_L)) {
        close_vgmstream(vgmstream);
        return NULL;
    }

    /* Channel 1 (Right) - If Stereo */
    if (channels == 2) {
        uint32_t offset_R = get_file_offset(sf, file_id_R, data_start);
        vgmstream->ch[1].streamfile = reopen_streamfile(sf, 0);
        if (!vgmstream->ch[1].streamfile) {
            close_vgmstream(vgmstream);
            return NULL;
        }
        vgmstream->ch[1].channel_start_offset = vgmstream->ch[1].offset = offset_R;
    }

    dsp_read_coefs_be(vgmstream, sf, entry_L + GCSP_ENT_COEFS, GCSP_ENTRY_SIZE);
    dsp_read_hist_be(vgmstream, sf, entry_L + GCSP_ENT_HIST, GCSP_ENTRY_SIZE);

    /* Parse Name Table */
    if (aux_count > 0) {

        /* String starts 4 bytes before the end of the last Name Entry */
        uint32_t string_start = name_table_start + (aux_count * GCSP_NAME_ENT_SIZE) - 0x04;
        uint32_t name_rel_offset = 0;

        int idx = target_subsong - 1;
        if (idx > 0) {
            /* Start Offset is the End Offset of the PREVIOUS name entry */
            uint32_t prev_name_ent = name_table_start + ((idx - 1) * GCSP_NAME_ENT_SIZE);
            name_rel_offset = read_u32be(prev_name_ent + GCSP_NAME_OFF_END_PTR, sf);
        }

        if ((string_start + name_rel_offset) < get_streamfile_size(sf)) {
            read_string(vgmstream->stream_name, STREAM_NAME_SIZE, string_start + name_rel_offset, sf);
        }
    }

    return vgmstream;
}