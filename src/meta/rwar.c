#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* RWAR - NintendoWare container [BIT.TRIP BEAT (Wii), Dance Dance Revolution Hottest Party 2 (Wii)] */
VGMSTREAM* init_vgmstream_rwar(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t tabl_offset, data_offset;
    uint32_t subfile_offset, subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "RWAR"))
        goto fail;

    if (!check_extensions(sf,"rwar"))
        goto fail;

    /* simple container of .rwavs (inside .brsar), rarely used with single siles (DDR) */

    /* abridged, see RWAV (same header) */
    /* 0x04(2): BOM */
    /* 0x06(2): version (usually 0100) */
    /* 0x08: file size */
    /* 0x0c(2): header size (0x20) */
    /* 0x0e(2): sections (2) */

    tabl_offset = read_u32be(0x10, sf);
    /* 0x14: tabl size */

    data_offset = read_u32be(0x18, sf);
    /* 0x1c: data size */

    /* TABL section */
    if (!is_id32be(tabl_offset + 0x00, sf, "TABL"))
        return NULL;
    /* 0x04: size */

    total_subsongs = read_u32be(tabl_offset + 0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* 0x00: always 0x01000000? */
    subfile_offset = read_u32be(tabl_offset + 0x0c + (target_subsong-1) * 0x0c + 0x04,sf) + data_offset;
    subfile_size   = read_u32be(tabl_offset + 0x0c + (target_subsong-1) * 0x0c + 0x08,sf);


    /* DATA section */
    if (!is_id32be(data_offset + 0x00, sf, "DATA"))
        goto fail;
    /* 0x04: size */

    //VGM_LOG("BRWAR: of=%x, sz=%x\n", subfile_offset, subfile_size);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "rwav");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_brwav(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
