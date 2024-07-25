#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* SNDP - from from Premium Agency games [Bakugan Battle Brawlers (PS3)] */
VGMSTREAM* init_vgmstream_sndp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    if (!is_id32be(0x00,sf, "SNDP"))
        return NULL;
    if (!check_extensions(sf,"past"))
        return NULL;

    bool loop_flag = (read_u32be(0x1c,sf) !=0);
    int channels = read_u16be(0x0c,sf);
    off_t start_offset = 0x30;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u32be(0x10,sf);
    vgmstream->num_samples = pcm16_bytes_to_samples(read_u32be(0x14,sf), channels);
    vgmstream->loop_start_sample = pcm16_bytes_to_samples(read_u32be(0x18,sf), channels);
    vgmstream->loop_end_sample = pcm16_bytes_to_samples(read_u32be(0x1C,sf), channels);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    vgmstream->meta_type = meta_SNDP;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
