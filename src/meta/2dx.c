#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* .2dx - Konami/Bemani beatmania IIDX container [beatmania IIDX 9th Style (AC) - beatmania IIDX 15 DJ TROOPERS (AC), Bishi Bashi Channel (AC)] */
VGMSTREAM* init_vgmstream_2dx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!check_extensions(sf, "2dx"))
        return NULL;

    // check for leftover crypto header (not part of the file):
    // - "%eNc" IIDX 9th Style
    // - "%e10" IIDX 10th Style
    // - "%e11" IIDX 11 RED
    // - "%e12" IIDX 12 HAPPY SKY
    // - "%hid" IIDX 15 DJ TROOPERS
    // - "%iO0" IIDX 16 EMPRESS
    uint32_t skip_offset = 0x00;
    uint32_t header_id = read_u32be(0x00, sf);
    uint32_t data_size = read_u32le(0x04, sf); // without padding (0x04 or 0x10)
    if ((header_id >> 24) == '%' && data_size + 0x10 > get_streamfile_size(sf)) {
        skip_offset = 0x08;
    }

    // 00: bank name
    // 10: first subsong offset 
    // 14: subsongs
    // 18+: memory garbage?
    // 48: table
    uint32_t table_offset = 0x48 + skip_offset;
    uint32_t first_offset = read_u32le(0x10 + skip_offset,sf);

    int target_subsong = sf->stream_index;
    int total_subsongs = read_u32le(0x14 + skip_offset,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs < 1) // arbitrary max
        return NULL;

    // extra checks to fail faster
    if (total_subsongs > 1024) // arbitrary max
        return NULL;
    if (first_offset != read_u32le(table_offset, sf))
        return NULL;

    uint32_t subfile_offset = read_u32le(table_offset + 0x04 * (target_subsong - 1), sf) + skip_offset;
    uint32_t subfile_size   = read_u32le(subfile_offset + 0x08, sf) + 0x18;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "2dx9");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_2dx9(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
