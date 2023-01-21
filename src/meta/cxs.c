#include "meta.h"
#include "../coding/coding.h"

/* CXS - tri-Crescendo games [Eternal Sonata (X360)] */
VGMSTREAM* init_vgmstream_cxs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "CXS "))
        goto fail;
    if (!check_extensions(sf,"cxs"))
        goto fail;

    loop_flag = read_32bitBE(0x18,sf) > 0;
    channels = read_32bitBE(0x0c,sf);
    start_offset = read_32bitBE(0x04,sf) + read_32bitBE(0x28,sf); /* assumed, seek table always at 0x800 */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /*  0x04: data start? */
    vgmstream->sample_rate = read_32bitBE(0x08,sf);
    vgmstream->num_samples = read_32bitBE(0x10,sf);
    vgmstream->loop_start_sample = read_32bitBE(0x14,sf);
    vgmstream->loop_end_sample = read_32bitBE(0x18,sf);
    /* 0x1c: below */

    vgmstream->meta_type = meta_CXS;

#ifdef VGM_USE_FFMPEG
    {
        uint32_t block_count = read_32bitBE(0x1c,sf);
        uint32_t block_size  = read_32bitBE(0x20,sf);
        uint32_t data_size   = read_32bitBE(0x24,sf);

        vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, start_offset, data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, block_count);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, 0, 0,1); /* num samples are ok */
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
