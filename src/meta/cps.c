#include "meta.h"
#include "../coding/coding.h"

/* CPS - tri-Crescendo games [Eternal Sonata (PS3)] */
VGMSTREAM* init_vgmstream_cps(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "CPS "))
        goto fail;
    if (!check_extensions(sf,"cps"))
        goto fail;

    start_offset = read_32bitBE(0x04,sf);
    channels = read_32bitBE(0x08,sf);
    loop_flag = read_32bitBE(0x18,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;
 
    vgmstream->meta_type = meta_CPS;
    vgmstream->channels = channels;
    vgmstream->sample_rate = read_32bitBE(0x10,sf);
    if (read_32bitBE(0x20,sf) == 0) {
        vgmstream->coding_type = coding_PCM16BE;
        vgmstream->layout_type = layout_interleave;
        vgmstream->num_samples = pcm16_bytes_to_samples(read_32bitBE(0x0c,sf), channels);
        vgmstream->interleave_block_size = 2;
    }
    else {
        vgmstream->coding_type = coding_PSX;
        vgmstream->layout_type = layout_interleave;
        vgmstream->num_samples = ps_bytes_to_samples(read_32bitBE(0x0c,sf), channels);
        vgmstream->interleave_block_size = 0x10;
        vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitBE(0x14,sf), channels);
        vgmstream->loop_end_sample = ps_bytes_to_samples(read_32bitBE(0x18,sf), channels);
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
