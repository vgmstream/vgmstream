#include "meta.h"
#include "../coding/coding.h"

/* .ESF - found in old Eurocom PC games [Mortal Kombat 4 (PC), Disney's Tarzan (PC)] */
VGMSTREAM* init_vgmstream_esf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t pcm_size;
    uint8_t version;
    off_t start_offset;
    int loop_flag = 0, bps_flag = 0, hq_flag = 0, codec_flag = 0,
        sample_rate, channels, bps;

    /* checks */
    if (!is_id32be(0x00, sf, "ESF\x03") &&
        !is_id32be(0x00, sf, "ESF\x06") &&
        !is_id32be(0x00, sf, "ESF\x08"))
        goto fail;

    if (!check_extensions(sf, "esf"))
        goto fail;

    version = read_u8(0x03, sf);
    pcm_size = read_u32le(0x04, sf);

    switch (version) {
        case 3:
            /* Disney's Hercules */
            sample_rate = read_u32le(0x08, sf);
            loop_flag = read_u8(0x0c, sf);
            bps_flag = read_u8(0x0d, sf);

            //bps = bps_flag ? 8 : 16;
            bps = 16;
            start_offset = 0x10;
            break;
        case 6:
            /* Mortal Kombat 4 */
            bps_flag = pcm_size & 0x20000000;
            hq_flag = pcm_size & 0x40000000;
            loop_flag = pcm_size & 0x80000000;
            pcm_size &= 0x1FFFFFFF;

            bps = bps_flag ? 16 : 8;
            sample_rate = hq_flag ? 22050 : 11025;
            start_offset = 0x08;
            break;
        case 8:
            /* Disney's Tarzan, Hydro Thunder */
            bps_flag = pcm_size & 0x10000000;
            hq_flag = pcm_size & 0x20000000;
            loop_flag = pcm_size & 0x40000000;
            codec_flag = pcm_size & 0x80000000;
            pcm_size &= 0x0FFFFFFF;

            bps = bps_flag ? 16 : 8;
            sample_rate = hq_flag ? 22050 : 11025;
            start_offset = 0x08;
            break;
        default:
            goto fail;
    }

    channels = 1; /* mono only */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ESF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_bytes_to_samples(pcm_size, channels, bps);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;
    vgmstream->layout_type = layout_none;

    switch (version) {
        case 3:
            vgmstream->coding_type = coding_DVI_IMA;
            break;
        case 6:
            vgmstream->coding_type = (bps == 8) ? coding_PCM8_U : coding_DVI_IMA;
            break;
        case 8:
            if (bps == 8) {
                vgmstream->coding_type = coding_PCM8_U;
            } else {
                vgmstream->coding_type = codec_flag ? coding_DVI_IMA : coding_PCM16LE;
            }
            break;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
