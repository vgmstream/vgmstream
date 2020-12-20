#include "meta.h"
#include "../coding/coding.h"


/* .ifs - Konami arcade games container [drummania (AC), GITADORA (AC)] */
VGMSTREAM* init_vgmstream_ifs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset, subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "ifs"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x6CAD8F89)
        goto fail;
    if (read_u16be(0x04,sf) != 0x0003)
        goto fail;

    /* .ifs format is a binary XML thing with types/fields/nodes/etc, that sometimes
     * contains Konami's BMP as subsongs (may differ in num_samples). This is an
     * abridged version of the whole thing, see:
     * - https://github.com/mon/ifstools
     * - https://github.com/mon/kbinxml
     * - https://bitbucket.org/ahigerd/gitadora2wav/
     */

    {
        off_t offset, size, subfile_start;

        subfile_start = read_u32be(0x10,sf);

        /* skip root section and point to childs */
        offset = 0x28 + 0x04 + read_u32be(0x28,sf);
        size = read_u32be(offset + 0x00,sf);
        offset += 0x04;

        /* point to subfile offsets */
        size   = size - 0x04 - read_u32be(offset + 0x00,sf) - 0x04;
        offset = offset + 0x04 + read_u32be(offset + 0x00,sf) + 0x04;

        total_subsongs = size / 0x0c;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        subfile_offset = read_u32be(offset + (target_subsong-1)*0x0c + 0x00,sf) + subfile_start;
        subfile_size   = read_u32be(offset + (target_subsong-1)*0x0c + 0x04,sf);
    }

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "bin");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_bmp_konami(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    /* subsongs have names but are packed in six-bit strings */

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
