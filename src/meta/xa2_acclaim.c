#include "meta.h"
#include "../coding/coding.h"


/* XA2 - from Acclaim games [RC Revenge Pro (PS2), XGIII: Extreme G Racing  (PS2)] */
VGMSTREAM* init_vgmstream_xa2_acclaim(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, sizes_offset;
    int channels, loop_flag = 0, interleave;


    /* checks */
    if (read_u32le(0x00,sf) > 0x10)
        return NULL;

    if (!check_extensions(sf, "xa2"))
        return NULL;

    channels = read_s32le(0x00, sf); /* seen +16 */
    loop_flag = 0;

    if (read_u32le(0x04,sf) > 0x1000) { /* RCRP (no interleave field) */
        interleave = (channels > 2) ? 0x400 : 0x1000;
        sizes_offset = 0x04;
    }
    else {
        interleave = read_s32le(0x04,sf);
        sizes_offset = 0x08;
    }

    /* N sizes that rougly sum data size (not all the same value), then empty */
    for (int i = 0; i < channels; i++) {
        if (read_32bitBE(sizes_offset + 0x04 * i, sf) == 0)
            goto fail;
    }

    if (read_32bitBE(sizes_offset + 0x04 * channels, sf) != 0)
        goto fail;

    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XA2_ACCLAIM;
    vgmstream->sample_rate = 44100;
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
