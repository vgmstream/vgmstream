#include "meta.h"
#include "../coding/coding.h"
#include "ppst_streamfile.h"


/* PPST - ParaPpa STream (maybe) [Parappa the Rapper (PSP)] */
VGMSTREAM* init_vgmstream_ppst(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "PPST"))
        return NULL;
    if (!check_extensions(sf, "sng"))
        return NULL;

    /* header has some control and comment fields then interleaved RIFF .at3 */

    /* count subsongs (mainly 4, rarely 1) */
    {
        off_t offset = 0xa0;

        total_subsongs = 0;
        while (offset < 0x800) {
            if (read_u32le(offset + 0x04, sf) == 0) /* subsong size */
                break;
            total_subsongs++;
            offset += 0x08;
        }

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    }

    {
        off_t start_offset = 0x800;
        size_t interleave_size = 0x4000;
        size_t stride_size = 0x4000 * total_subsongs;
        /* subsong header at 0xa0, 0x00(1): id, 0x01(3): blocks of interleave */
        size_t stream_size = read_u32le(0xA0 + 0x08 * (target_subsong - 1) + 0x04, sf);

        STREAMFILE* temp_sf = setup_ppst_streamfile(sf, start_offset + interleave_size * (target_subsong - 1), interleave_size, stride_size, stream_size);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_riff(temp_sf);
        close_streamfile(temp_sf);
        if (!vgmstream) goto fail;

        vgmstream->num_streams = total_subsongs;
        vgmstream->stream_size = stream_size;
        vgmstream->meta_type = meta_PPST;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
