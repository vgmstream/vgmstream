#include "meta.h"
#include "../coding/coding.h"
#include "mups_streamfile.h"


/* MUPS - from Watermelon/HUCARD games (same programmer) [Pier Solar and the Great Architects (PC), Ghost Blade HD (PC/Switch)] */
VGMSTREAM* init_vgmstream_mups(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "MUPS"))
        goto fail;

    /* mups: header id? 
     * (extensionless): default? */
    if (!check_extensions(sf, "mups,"))
        goto fail;

    if (!is_id32be(0x08,sf, "PssH"))
        goto fail;

    /* just an Ogg with changed OggS/vorbis words (see streamfile) */
    temp_sf = setup_mups_streamfile(sf, 0x08);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ogg_vorbis(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
