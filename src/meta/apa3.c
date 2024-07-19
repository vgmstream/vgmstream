#include "meta.h"
#include "../coding/coding.h"
#include "apa3_streamfile.h"


/* .ATX - Media.Vision's segmented RIFF AT3 wrapper [Senjo no Valkyria 3 (PSP), Shining Blade (PSP)] */
VGMSTREAM* init_vgmstream_apa3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "APA3"))
        return NULL;
    if (!check_extensions(sf,"atx"))
        return NULL;

    /* .ATX is made of subfile segments, handled by a custom SF.
     * Each segment has a header/footer, and part of the full AT3 data
     * (i.e. ATRAC3 data ends in a subfile and continues in the next) */
    temp_sf = setup_apa3_streamfile(sf);
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
