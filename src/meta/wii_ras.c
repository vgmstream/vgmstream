#include "meta.h"
#include "../coding/coding.h"

/* RAS_ - from Donkey Kong Country Returns (Wii) */
VGMSTREAM* init_vgmstream_ras(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "RAS_"))
        return NULL;
    if (!check_extensions(sf, "ras"))
        return NULL;

    loop_flag = 0;
    if (read_u32be(0x30,sf) != 0 ||
        read_u32be(0x34,sf) != 0 ||
        read_u32be(0x38,sf) != 0 ||
        read_u32be(0x3C,sf) != 0) {
        loop_flag = 1;
    }
    channels = 2;
    start_offset = read_u32be(0x18,sf);
    int interleave = read_u32be(0x20,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x14,sf);
    vgmstream->meta_type = meta_RAS;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    vgmstream->num_samples = dsp_bytes_to_samples(read_u32be(0x1c,sf), channels);
    if (loop_flag) { /* loop is block + samples into block */
        vgmstream->loop_start_sample = dsp_bytes_to_samples(read_u32be(0x30,sf) * interleave, 1) + read_s32be(0x34,sf);
        vgmstream->loop_end_sample   = dsp_bytes_to_samples(read_u32be(0x38,sf) * interleave, 1) + read_s32be(0x3C,sf);
    }

    dsp_read_coefs_be(vgmstream,sf,0x40,0x30);


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
