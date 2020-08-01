#include "meta.h"
#include "../coding/coding.h"

/* .zwv - from Namco games [THE iDOLM@STER Shiny TV (PS3)] */
VGMSTREAM* init_vgmstream_zwv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;


    /* checks */
    if (!check_extensions(sf,"zwv"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x77617665) /* "wave" */
        goto fail;

    /* has a mini header then a proper MSF:
     * 0x04: null
     * 0x08: null
     * 0x0c: version/config? (0x06040000)
     * 0x10: version/config? (0x00030210)
     * 0x14: sample rate
     * 0x18: ? (related to sample rate)
     * 0x1c: null
     * 0x20: data offset
     * 0x24: data size
     * 0x28: loop flag (0x30+ is removed if no loop)
     * 0x2c: ? (related to loop, or null)
     * 0x30: null
     * 0x30: loop start offset (same as MSF)
     * 0x30: loop end offset (same as MSF start+length)
     * 0x3c: null
     */

    subfile_offset = read_u32be(0x20, sf) - 0x40;
    subfile_size   = read_u32be(0x24, sf) + 0x40;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "msf");
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
