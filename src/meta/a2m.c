#include "meta.h"
#include "../coding/coding.h"

/* A2M - from Artificial Mind & Movement games [Scooby-Doo! Unmasked (PS2)] */
VGMSTREAM* init_vgmstream_a2m(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "A2M\0"))
        return NULL;
    if (!is_id32be(0x04,sf, "PS2\0"))
        return NULL;
    if (!check_extensions(sf,"int") )
        return NULL;

    start_offset = 0x30;
    data_size = get_streamfile_size(sf) - start_offset;
    channels = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_A2M;
    vgmstream->sample_rate = read_s32be(0x10,sf);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x6000;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
