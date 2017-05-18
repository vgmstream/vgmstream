#include "meta.h"
#include "../coding/coding.h"

/* XMA - Microsoft format derived from WMAPRO, found in X360/XBone games */
VGMSTREAM * init_vgmstream_xma(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset, first_offset = 0xc;
    size_t data_size, chunk_size;
    int loop_flag, channel_count, sample_rate, is_xma2_old = 0, is_xma1 = 0;
    int num_samples, loop_start_sample, loop_end_sample, loop_start_b = 0, loop_end_b = 0, loop_subframe = 0;


    /* check extension, case insensitive */
    /* .xma2: Skullgirls, .nps: Beautiful Katamari (renamed .xma), .str: Sonic & Sega All Stars Racing */
    if ( !check_extensions(streamFile, "xma,xma2,nps,str") )
        goto fail;

    {
        size_t file_size = streamFile->get_size(streamFile);
        size_t riff_size = read_32bitLE(0x04,streamFile);
        /* +8 for some Beautiful Katamari files, unsure if bad rip */
        if (riff_size != file_size && riff_size+8 > file_size)
            goto fail;
    }


    /* parse LE RIFF header, with "XMA2" (XMA2WAVEFORMAT) or "fmt " (XMAWAVEFORMAT/XMA2WAVEFORMAT) main chunks
     * Often comes with an optional "seek" chunk too */

    /* parse sample data */
    if ( find_chunk_le(streamFile, 0x584D4132,first_offset,0, &chunk_offset,&chunk_size) ) { /* old XMA2 */
        is_xma2_old = 1;
        xma2_parse_xma2_chunk(streamFile, chunk_offset, &channel_count,&sample_rate, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample);
    }
    else if ( find_chunk_le(streamFile, 0x666d7420,first_offset,0, &chunk_offset,&chunk_size)) {
        int format = read_16bitLE(chunk_offset,streamFile);
        if (format == 0x165) { /* XMA1 */
            is_xma1 = 1;
            xma1_parse_fmt_chunk(streamFile, chunk_offset, &channel_count,&sample_rate, &loop_flag, &loop_start_b, &loop_end_b, &loop_subframe, 0);
        } else if (format == 0x166) { /* new XMA2 */
            channel_count = read_16bitLE(chunk_offset+0x02,streamFile);
            sample_rate   = read_32bitLE(chunk_offset+0x04,streamFile);
            xma2_parse_fmt_chunk_extra(streamFile, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 0);
        } else {
            goto fail;
        }
    }
    else {
        goto fail;
    }

    /* "data" chunk */
    if (!find_chunk_le(streamFile, 0x64617461,first_offset,0, &start_offset,&data_size)) /*"data"*/
        goto fail;


    /* fix samples; for now only XMA1 is fixed, but xmaencode.exe doesn't seem to use XMA2
     * num_samples in the headers, and the values don't look exact */
    if (is_xma1) {
        ms_sample_data msd;
        memset(&msd,0,sizeof(ms_sample_data));

        msd.xma_version = 1;
        msd.channels    = channel_count;
        msd.data_offset = start_offset;
        msd.data_size   = data_size;
        msd.loop_flag   = loop_flag;
        msd.loop_start_b= loop_start_b;
        msd.loop_end_b  = loop_end_b;
        msd.loop_start_subframe = loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */

        xma_get_samples(&msd, streamFile);

        num_samples = msd.num_samples;
        //skip_samples = msd.skip_samples;
        loop_start_sample = msd.loop_start_sample;
        loop_end_sample = msd.loop_end_sample;
        /* XMA2 loop/num_samples don't seem to skip_samples */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_XMA_RIFF;


#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;

        if (is_xma2_old) {
            bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile);
        } else {
            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile, 0);
        }
        if (bytes <= 0) goto fail;

        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

#if 0
    //not active due to a FFmpeg bug that misses some of the last packet samples and decodes
    // garbage if asked for more samples (always happens but more apparent with skip_samples active)
    /* fix encoder delay */
    if (data->skipSamples==0)
        ffmpeg_set_skip_samples(data, xma.skip_samples);
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


#if 0
/**
 * Get real XMA sample rate (from Microsoft docs).
 * Info only, not for playback as the encoder adjusts sample rate for looping purposes (sample<>data align).
 */
static int32_t get_xma_sample_rate(int32_t general_rate) {
    int32_t xma_rate = 48000; /* default XMA */

    if (general_rate <= 24000)      xma_rate = 24000;
    else if (general_rate <= 32000) xma_rate = 32000;
    else if (general_rate <= 44100) xma_rate = 44100;

    return xma_rate;
}
#endif
