#include "meta.h"
#include "sscf_encrypted.h"

/* SSCF - Square-Enix games, older version of .scd with encrypted data [Final Fantasy XI (360), PlayOnline Viewer (X360)] */
VGMSTREAM* init_vgmstream_sscf_encrypted(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t xorkey;


    /* checks */
    if (!is_id32be(0x00,sf, "SSCF"))
        goto fail;
    if (!check_extensions(sf, "scd"))
        goto fail;

    /* LE header even though X360 */
    /* 0x04: version? (0x0003xxxx)*/
    /* 0x08: file size, except for a few files that with a weird value */
    /* 0x0c: null */

    /* 0x10: file id */
    xorkey = read_u32le(0x14,sf);
    /* 0x18: null */
    /* 0x1c: always 1 */
    /* 0x20~0x40: null */

    /* 0x40: num samples? */
    /* 0x48: loop start? */
    /* 0x50: channels */
    /* 0x54: sample rate */
    /* rest: null */

    /* 0x80: encrypted RIFF data */

    temp_sf = setup_sscf_streamfile(sf, xorkey);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_xma(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
