#include "meta.h"
#include "../coding/coding.h"

/* .afs - CRI container [Sonic Gems Collection (PS2)] */
VGMSTREAM* init_vgmstream_afs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, subfile_id;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "AFS\0"))
        return NULL;

    total_subsongs = read_u32le(0x04,sf);
    if (total_subsongs > 256 || total_subsongs == 0) /* arbitrary max */
        return NULL;

    if (!check_extensions(sf,"afs"))
        return NULL;

    init_vgmstream_t init_vgmstream = NULL;
    const char* fake_ext = NULL;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    subfile_offset = read_u32le(0x08 + (target_subsong-1) * 0x08 + 0x00,sf);
    subfile_size   = read_u32le(0x08 + (target_subsong-1) * 0x08 + 0x04,sf);

    subfile_id = read_u32be(subfile_offset,sf);
    if ((subfile_id & 0xFFFF0000) == 0x80000000) {
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
