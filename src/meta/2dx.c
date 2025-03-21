#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* .2dx - Konami/Bemani beatmania IIDX container [beatmania IIDX 9th Style (AC) - beatmania IIDX 15 DJ TROOPERS (AC)] */
VGMSTREAM* init_vgmstream_2dx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int target_subsong = sf->stream_index, total_subsongs;
    uint32_t meta_offset, table_offset, subfile_offset, subfile_size;


    /* checks */
    if (!check_extensions(sf, "2dx"))
        goto fail;

    /* Check for leftover crypto header */ 
    /*
    if (read_u32be(0x00, sf) == 0x25654E63 || //IIDX 9th Style
        read_u32be(0x00, sf) == 0x25653130 || //IIDX 10th Style
        read_u32be(0x00, sf) == 0x25653131 || //IIDX 11 RED
        read_u32be(0x00, sf) == 0x25653132 || //IIDX 12 HAPPY SKY
        read_u32be(0x00, sf) == 0x25686964 || //IIDX 15 DJ TROOPERS
        read_u32be(0x00, sf) == 0x25694F30)   //IIDX 16 EMPRESS
        meta_offset = 0x18;
    else */
        meta_offset = 0x10;
    table_offset = meta_offset + 0x38;

    if (target_subsong == 0) target_subsong = 1;
    total_subsongs = read_u32le(meta_offset + 0x4,sf);
    if (target_subsong > total_subsongs) 
        goto fail;

    subfile_offset = read_u32le(table_offset + 0x04 * (target_subsong - 1), sf);
    subfile_size   = read_u32le(subfile_offset + 0x8,sf) + 0x18;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "2dx9");
    if (!temp_sf) goto fail;

    temp_sf->stream_index = 1;
    vgmstream = init_vgmstream_2dx9(temp_sf);
    if (!vgmstream) goto fail;

    if (vgmstream->num_streams > 1)
        goto fail;
    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
