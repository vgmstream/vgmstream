#include "meta.h"
#include "../coding/coding.h"

/* CXS - found in Eternal Sonata (Xbox 360) */
VGMSTREAM * init_vgmstream_x360_cxs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"cxs"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x43585320)   /* "CXS " */
        goto fail;

    loop_flag = read_32bitBE(0x18,streamFile) > 0;
    channel_count = read_32bitBE(0x0c,streamFile);

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    start_offset = read_32bitBE(0x04,streamFile) + read_32bitBE(0x28,streamFile); /* assumed, seek table always at 0x800 */
    /*  0x04: data start? */
    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->channels = channel_count; /*0x0c*/
    vgmstream->num_samples = read_32bitBE(0x10,streamFile) + 576; /*todo add proper encoder_delay*/
    vgmstream->loop_start_sample = read_32bitBE(0x14,streamFile);
    vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile);
    /* 0x1c: below */

    vgmstream->meta_type = meta_X360_CXS;

#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data *ffmpeg_data = NULL;
        uint8_t buf[100];
        size_t bytes, datasize, block_size, block_count;

        block_count = read_32bitBE(0x1c,streamFile);
        block_size  = read_32bitBE(0x20,streamFile);
        datasize    = read_32bitBE(0x24,streamFile);

        bytes = ffmpeg_make_riff_xma2(buf,100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
        if (bytes <= 0) goto fail;

        ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
        if ( !ffmpeg_data ) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
