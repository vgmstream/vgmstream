#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* .ssp - Konami/Bemani beatmania IIDX container [beatmania IIDX 16 (AC) ~ beatmania IIDX 19 (AC), Bishi Bashi Channel (AC)] */
VGMSTREAM* init_vgmstream_ssp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!check_extensions(sf, "ssp"))
        return NULL;


    // 00: bank name
    // 10: first subsong offset 
    // 14: max subsongs
    // 18+: memory garbage?
    // 48: table
    uint32_t table_offset = 0x48;
    uint32_t first_offset = read_u32le(0x10,sf);

    int target_subsong = sf->stream_index;
    int total_subsongs = read_u32le(0x14,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs < 1) // arbitrary max
        return NULL;

    // extra checks to fail faster
    if (total_subsongs > 1024) // arbitrary max
        return NULL;
    if (first_offset != read_u32le(table_offset, sf))
        return NULL;

    // unlike .2dx table seems to have many blanks (total subsongs seems accurate)
    uint32_t subfile_offset = 0;
    int current_subsong = 0;
    uint32_t offset = table_offset;
    while (offset < first_offset) {
        uint32_t entry_offset = read_u32le(offset, sf);
        offset += 0x04;

        if (entry_offset == 0)
            continue;
        current_subsong++;

        if (current_subsong == target_subsong) {
            subfile_offset = entry_offset;
            break;
        }
    }
    
    if (subfile_offset == 0)
        return NULL;

    uint32_t subfile_size   = read_u32le(subfile_offset + 0x08, sf) + 0x18;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "sd9");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_sd9(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
