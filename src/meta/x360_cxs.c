#include "meta.h"
#include "../coding/coding.h"

/* CXS - found in Eternal Sonata (X360) */
VGMSTREAM* init_vgmstream_x360_cxs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    if ( !check_extensions(sf,"cxs"))
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x43585320)   /* "CXS " */
        goto fail;

    loop_flag = read_32bitBE(0x18,sf) > 0;
    channel_count = read_32bitBE(0x0c,sf);
    start_offset = read_32bitBE(0x04,sf) + read_32bitBE(0x28,sf); /* assumed, seek table always at 0x800 */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /*  0x04: data start? */
    vgmstream->sample_rate = read_32bitBE(0x08,sf);
    vgmstream->num_samples = read_32bitBE(0x10,sf);
    vgmstream->loop_start_sample = read_32bitBE(0x14,sf);
    vgmstream->loop_end_sample = read_32bitBE(0x18,sf);
    /* 0x1c: below */

    vgmstream->meta_type = meta_X360_CXS;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes, datasize, block_size, block_count;

        block_count = read_32bitBE(0x1c,sf);
        block_size  = read_32bitBE(0x20,sf);
        datasize    = read_32bitBE(0x24,sf);

        bytes = ffmpeg_make_riff_xma2(buf,100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
        if (bytes <= 0) goto fail;

        vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,datasize);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset,datasize, 0, 0,1); /* num samples are ok */
    }
#else
    goto fail;
#endif

    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
