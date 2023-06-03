#include "meta.h"
#include "../coding/coding.h"
#include "sfh_streamfile.h"

typedef VGMSTREAM* (*init_vgmstream_t)(STREAMFILE*);


/* .SFH - Capcom wrapper used with common audio extensions [Devil May Cry 4 Demo (PS3), Jojo's Bizarre Adventure HD (PS3), Sengoku Basara 4 Sumeragi (PS3)] */
VGMSTREAM* init_vgmstream_sfh(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    init_vgmstream_t init_vgmstream = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "\0SFH"))
        return NULL;
    if (!check_extensions(sf,"at3,sspr"))
        return NULL;

    /* mini header */
    uint32_t version     = read_u32be(0x04,sf);
    uint32_t clean_size  = read_u32be(0x08,sf); /* there is padding data at the end */
    /* 0x0c: always 0 */

    char* extension;
    uint32_t header_id = read_u32be(0x10,sf);
    switch(header_id) {
        case 0x52494646: // RIFF
            init_vgmstream = init_vgmstream_riff;
            extension = "at3";
            break;

        case 0x53535052: // SSPR
            init_vgmstream = init_vgmstream_sspr;
            extension = "sspr";
            break;

        case 0x00434C44: // \0CLD (.dlcp)
        case 0x00435241: // \0CRA (.arc)
        default:
            goto fail;
    }

    uint32_t block_size;
    switch(version) {
        case 0x00010000: block_size = 0x10010; break; /* DMC4 Demo (not retail) */
        case 0x00010001: block_size = 0x20000; break; /* Jojo, SB4 */
        default: goto fail;
    }

    temp_sf = setup_sfh_streamfile(sf, 0x00, block_size, clean_size, extension);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
