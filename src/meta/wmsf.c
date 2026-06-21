#include "meta.h"
#include "../coding/coding.h"

/* WMSF - Banpresto MSFx wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS3)] */
VGMSTREAM* init_vgmstream_wmsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!is_id32be(0x00,sf,"WMSF"))
        return NULL;
    if (!check_extensions(sf,"msf"))
        return NULL;
    // 0x04: size
    // 0x08: flags?
    // 0x0c: null?
    
    uint32_t subfile_offset = 0x10;
    uint32_t subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_msf(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
