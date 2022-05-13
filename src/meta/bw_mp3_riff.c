#include "meta.h"
#include "../coding/coding.h"


/* Bioware pseudo-format (for sfx) [Star Wars: Knights of the Old Republic 1/2 (PC/Switch/iOS)] */
VGMSTREAM* init_vgmstream_bw_mp3_riff(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset = 0, subfile_size = 0;


    /* checks */
    if (read_u32be(0x00, sf) != 0xFFF360C4)
        goto fail;
    //if ((read_u32be(0x00, sf) & 0xFFF00000) != 0xFFF00000) /* no point to check other mp3s */
    //    goto fail;
    if (!is_id32be(0x0d, sf, "LAME") && !is_id32be(0x1d6, sf, "RIFF"))
        goto fail;

    /* strange mix of micro empty MP3 (LAME3.93, common header) + standard RIFF */

    subfile_offset = 0x1d6;
    subfile_size = read_u32le(subfile_offset + 0x04, sf) + 0x08;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "wav");
    if (!temp_sf) goto fail;

    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_riff(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Bioware pseudo-format (for music) [Star Wars: Knights of the Old Republic 1/2 (PC/Switch/iOS)] */
VGMSTREAM* init_vgmstream_bw_riff_mp3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset = 0, subfile_size = 0;


    /* checks */
    if (!is_id32be(0x00, sf, "RIFF"))
        goto fail;
    if (read_u32le(0x04, sf) != 0x32)
        goto fail;
    if (get_streamfile_size(sf) <= 0x32 + 0x08) /* ? */
        goto fail;

    /* strange mix of micro RIFF (with codec 0x01, "fact" and "data" size 0)) + standard MP3 (sometimes with ID3) */

    subfile_offset = 0x3A;
    subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "mp3");
    if (!temp_sf) goto fail;

    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_mpeg(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
