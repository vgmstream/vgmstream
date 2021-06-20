#include "meta.h"
#include "../coding/coding.h"

/* WMSF - Banpresto MSFx wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS3)] */
VGMSTREAM* init_vgmstream_msf_banpresto_wmsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset = 0x10;
    size_t subfile_size = get_streamfile_size(sf) - subfile_offset;


    /* checks */
    if (!check_extensions(sf,"msf"))
        goto fail;
    if (!is_id32be(0x00,sf,"WMSF"))
        goto fail;
    /* 0x04: size, 0x08: flags? 0x0c: null? */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, NULL);
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

/* 2MSF - Banpresto RIFF wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS4)] */
VGMSTREAM* init_vgmstream_msf_banpresto_2msf(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE*temp_sf = NULL;
    off_t subfile_offset = 0x14;
    size_t subfile_size = get_streamfile_size(sf) - subfile_offset;


    /* checks */
    if ( !check_extensions(sf,"at9"))
        goto fail;
    if (!is_id32be(0x00,sf,"2MSF"))
        goto fail;
    /* 0x04: size, 0x08: flags? 0x0c: null?, 0x10: 0x01? (BE values even though RIFF is LE) */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, NULL);
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
