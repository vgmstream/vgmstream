#include "meta.h"
#include "../coding/coding.h"

/* ASTB - found in Dead Rising (X360) */
VGMSTREAM * init_vgmstream_x360_ast(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag, channel_count;
    int i, xma_streams;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"ast"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x41535442) /* "ASTB" */
        goto fail;

    if (read_32bitBE(0x04,streamFile) != get_streamfile_size(streamFile)) goto fail;
    if (read_16bitBE(0x30,streamFile) != 0x165) goto fail; /* only seen XMA1 */

    xma_streams = read_16bitBE(0x38,streamFile);

    loop_flag = read_8bit(0x3a,streamFile);
    channel_count = 0; /* sum of all stream channels (though only 1/2ch ever seen) */
    for (i = 0; i < xma_streams; i++) {
        channel_count += read_8bit(0x3c + 0x14*i + 0x11,streamFile);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    start_offset = read_32bitBE(0x10,streamFile);
    data_size = read_32bitBE(0x20,streamFile);

    vgmstream->sample_rate = read_32bitBE(0x40,streamFile);
    vgmstream->meta_type = meta_X360_AST;

    {
        /* manually find sample offsets (XMA1 nonsense again) */
        ms_sample_data msd = {0};

        msd.xma_version = 1;
        msd.channels = channel_count;
        msd.data_offset = start_offset;
        msd.data_size = data_size;
        msd.loop_flag = loop_flag;
        msd.loop_start_b = read_32bitBE(0x44,streamFile);
        msd.loop_end_b   = read_32bitBE(0x48,streamFile);
        msd.loop_start_subframe = read_8bit(0x4c,streamFile) & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = read_8bit(0x4c,streamFile) >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */

        xma_get_samples(&msd, streamFile);
        vgmstream->num_samples = msd.num_samples;
        vgmstream->loop_start_sample = msd.loop_start_sample;
        vgmstream->loop_end_sample = msd.loop_end_sample;
    }

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[100];
        size_t bytes;

        off_t fmt_offset = 0x30;
        size_t fmt_size = 0x0c + xma_streams * 0x14;

        /* XMA1 "fmt" chunk @ 0x20 (BE, unlike the usual LE) */
        bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,100, fmt_offset,fmt_size, data_size, streamFile, 1);
        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, streamFile, start_offset, data_size, fmt_offset, 1,1);
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
