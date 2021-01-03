#include "meta.h"
#include "../coding/coding.h"

/* .acx - CRI container [Baroque (SAT), Persona 3 (PS2), THE iDOLM@STER: Live For You (X360)] */
VGMSTREAM* init_vgmstream_acx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf,"acx"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x00000000)
        goto fail;

    /* simple container for sfx and rarely music [Burning Rangers (SAT)],
     * mainly used until .csb was introduced */

    total_subsongs = read_u32be(0x04,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    subfile_offset = read_u32be(0x08 + (target_subsong-1) * 0x08 + 0x00,sf);
    subfile_size   = read_u32be(0x08 + (target_subsong-1) * 0x08 + 0x04,sf);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "adx");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_adx(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
