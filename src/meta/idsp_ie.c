#include "meta.h"
#include "../coding/coding.h"


/* IDSP - from Inevitable Entertainment games [Defender (GC)] */
VGMSTREAM* init_vgmstream_idsp_ie(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00, sf, "IDSP"))
        return NULL;
    if (!check_extensions(sf,"idsp"))
        return NULL;

    channels = read_s32be(0x0C,sf);
    if (channels > 2) return NULL;

    loop_flag = 0;
    start_offset = 0x70;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IDSP_IE;
    vgmstream->sample_rate = read_s32be(0x08,sf);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = dsp_bytes_to_samples(read_u32be(0x04,sf), channels);

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_u32be(0x10,sf);
    dsp_read_coefs_be(vgmstream, sf, 0x14, 0x2E);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
