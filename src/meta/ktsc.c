#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* KTSC - Koei Tecmo KTSR container */
VGMSTREAM* init_vgmstream_ktsc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int target_subsong = sf->stream_index, total_subsongs;
    uint32_t offset, subfile_offset, subfile_size;


    /* checks */
    if (!is_id32be(0x00, sf, "KTSC"))
        goto fail;
    if (read_u32be(0x04, sf) != 0x01000001) /* version? */
        goto fail;

    /* .ktsl2asbin: common [Atelier Ryza (PC)]
     * .asbin: Warriors Orochi 4 (PC) (assumed) */
    if (!check_extensions(sf, "ktsl2asbin,asbin"))
        goto fail;

    /* KTSC is a container of KTSRs, but can't be extracted easily as they use absolute pointers to the
     * same stream companion file. KTSRs may have subsongs, but only seem to have 1, so use KTSC's subsongs. */

    if (target_subsong == 0) target_subsong = 1;
    total_subsongs = read_u32le(0x08, sf);
    if (target_subsong > total_subsongs)
        goto fail;

    /* 0x0c: CRC(?) table start */
    offset = read_u32le(0x10, sf);
    /* 0x14: file size */
    /* 0x18: header end */
    /* 0x1c: null */
    /* 0x20+: CRC(?) table, 1 entry per file */

    subfile_offset = read_u32le(offset + 0x04 * (target_subsong - 1), sf);
    subfile_size = read_u32le(subfile_offset + 0x1c, sf); /* from header, meh */

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    temp_sf->stream_index = 1;
    vgmstream = init_vgmstream_ktsr(temp_sf);
    if (!vgmstream) goto fail;

    if (vgmstream->num_streams > 1)
        goto fail;
    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
