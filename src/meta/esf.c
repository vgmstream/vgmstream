#include "meta.h"
#include "../coding/coding.h"

/* .ESF - from Mortal Kombat 4 (PC) */
VGMSTREAM* init_vgmstream_esf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t pcm_size;
    off_t start_offset;
    int loop_flag, bps_flag, hq_flag, channels, bps;

    /* checks */
    if (!is_id32be(0x00, sf, "ESF\x06"))
        goto fail;

    if (!check_extensions(sf, "esf"))
        goto fail;

    pcm_size = read_u32le(0x04, sf);
    bps_flag = pcm_size & 0x20000000;
    hq_flag = pcm_size & 0x40000000;
    loop_flag = pcm_size & 0x80000000;
    pcm_size &= 0x1FFFFFFF;

    channels = 1; /* mono only */
    start_offset = 0x08;
    bps = bps_flag ? 16 : 8; /* 16 is supposed to mean PCM16 but is actually IMA */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ESF;
    vgmstream->sample_rate = hq_flag ? 22050 : 11025;
    vgmstream->coding_type = (bps == 8) ? coding_PCM8_U : coding_DVI_IMA;
    vgmstream->num_samples = pcm_bytes_to_samples(pcm_size, 1, bps);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
