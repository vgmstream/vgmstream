#include "meta.h"
#include "../coding/coding.h"

/* DERF - from Stupid Invaders (PC) */
VGMSTREAM* init_vgmstream_derf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    size_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "DERF"))
        return NULL;
    if (!check_extensions(sf, "adp"))
        return NULL;

    channels = read_s32le(0x04,sf);
    if (channels > 2) return NULL;
    /* movie DERF also exist with slightly different header */

    start_offset = 0x0c;
    data_size = read_u32le(0x08,sf);
    if (data_size + start_offset != get_streamfile_size(sf))
        return NULL;

    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 22050;
    vgmstream->meta_type = meta_DERF;
    vgmstream->coding_type = coding_DERF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->num_samples = data_size / channels; /* bytes-to-samples */

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
