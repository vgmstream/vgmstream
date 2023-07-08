#include "meta.h"
#include "../coding/coding.h"

/* hgC1 - from Knights of the Temple 2 (PS2) */
VGMSTREAM* init_vgmstream_hgc1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag = 0;

    /* checks */
    if (!is_id32be(0x00,sf, "hgC1"))
        return NULL;
    if (!is_id32be(0x04,sf, "strm"))
        return NULL;
    if (!check_extensions(sf,"str"))
        return NULL;

    start_offset = 0x20;
    loop_flag = 0;
    channels = read_s32le(0x08,sf); /* always stereo? */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HGC1;
    vgmstream->sample_rate = read_s32le(0x10,sf);
    vgmstream->num_samples = ps_bytes_to_samples(read_u32le(0x0C,sf) * 0x10, 1); /* mono frames*/

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
