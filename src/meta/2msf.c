#include "meta.h"
#include "../coding/coding.h"

/* 2MSF - Banpresto RIFF wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS4)] */
VGMSTREAM* init_vgmstream_2msf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!is_id32be(0x00,sf,"2MSF"))
        return NULL;
    if (!check_extensions(sf,"at9"))
        return NULL;
    // 0x04: size
    // 0x08: flags?
    // 0x0c: null?
    // 0x10: 0x01? (BE values even though RIFF is LE)

    uint32_t subfile_offset = 0x14;
    uint32_t subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_riff(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
