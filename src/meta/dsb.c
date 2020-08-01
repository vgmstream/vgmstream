#include "meta.h"
#include "../coding/coding.h"

/* .dsb - from Namco games [Taiko no Tatsujin DS: Dororon! Yokai Daikessen!! (DS)] */
VGMSTREAM* init_vgmstream_dsb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;


    /* checks */
    if (!check_extensions(sf,"dsb"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x44535342) /* "DSSB" */
        goto fail;
    if (read_u32be(0x40,sf) != 0x44535354) /* "DSST" */
        goto fail;

    /* - DDSB:
     * 0x04: chunk size
     * 0x08: file name
     * 0x14: sample rate
     * 0x18: v01?
     * 0x1c: file size
     * 0x20: DSST offset
     *
     * - DDST:
     * 0x44: chunk size
     * 0x48: file name
     * 0x58: small signed number?
     * 0x5c: data size (with padding)
     * 0x60: small signed number?
     * 0x64: ?
     * rest: null
     */

    subfile_offset = 0x80;
    subfile_size   = read_u32be(0x80 + 0x04, sf) + 0x08; /* files are padded so use BNSF */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "bnsf");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_bnsf(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
