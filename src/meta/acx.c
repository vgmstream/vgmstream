#include "meta.h"
#include "../coding/coding.h"

/* .acx - CRI container [Baroque (SAT), Persona 3 (PS2), THE iDOLM@STER: Live For You (X360)] */
VGMSTREAM* init_vgmstream_acx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, subfile_id;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (read_u32be(0x00,sf) != 0x00000000)
        return NULL;

    /* simple container for sfx and rarely music [Burning Rangers (SAT)],
     * mainly used until .csb was introduced */
    total_subsongs = read_u32be(0x04,sf);
    if (total_subsongs > 256 || total_subsongs == 0) /* arbitrary max */
        return NULL;

    if (!check_extensions(sf,"acx"))
        return NULL;

    init_vgmstream_t init_vgmstream = NULL;
    const char* fake_ext = NULL;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    subfile_offset = read_u32be(0x08 + (target_subsong-1) * 0x08 + 0x00,sf);
    subfile_size   = read_u32be(0x08 + (target_subsong-1) * 0x08 + 0x04,sf);

    subfile_id = read_u32be(subfile_offset,sf);
    if (subfile_id == get_id32be("OggS")) { /* 12Riven (PC) */
        init_vgmstream = init_vgmstream_ogg_vorbis;
        fake_ext = "ogg";
    }
    else if ((subfile_id & 0xFFFF0000) == 0x80000000) {
        init_vgmstream = init_vgmstream_adx;
        fake_ext = "adx";
    }
    else {
        goto fail;
    }

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, fake_ext);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
