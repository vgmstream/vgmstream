#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

/* UNDEFIND - Kylotonn games wrapper [Hunter's Trophy 2 (multi), WRC 5 (multi)] */
VGMSTREAM* init_vgmstream_undefind(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id64be(0x00,sf,"UNDEFIND")) //odd but consistent
        return NULL;
    if (!check_extensions(sf,"paf"))
        return NULL;

    read_u32_t read_u32 = guess_read_u32(0x0c, sf);

    // 08: IDNE in machine endianness
    // 0c: always 3 (version?)
    // 10: null
    // 14: null
    int name_size = read_u32(0x18, sf);
    // 1c: platform string (WIIU, PSVITA, PS3, PC, X360)
    uint32_t offset = 0x1c + name_size;

    uint32_t subfile_offset = read_u32(offset + 0x00, sf) + offset + 0x04;
    // 04: always 1 (subsongs? internal FSB is always single stream)
    uint32_t subfile_size   = read_u32(offset + 0x08, sf);
    // 0c: 2 or 7?
    // 10: empty
    // 20: 50.0?
    // 24: 500.0?

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "fsb");
    if (!temp_sf) goto fail;

    // no apparent flag
    if (is_id32be(subfile_offset, sf, "FSB4")) { // The Cursed Crusade (multi), Hunter's Trophy (multi)
        vgmstream = init_vgmstream_fsb(temp_sf);
        if (!vgmstream) goto fail;
    }
    else if (is_id32be(subfile_offset, sf, "FSB5")) { // WRC 5 (multi)
        vgmstream = init_vgmstream_fsb5(temp_sf);
        if (!vgmstream) goto fail;
    }
    else {
        goto fail;
    }

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
