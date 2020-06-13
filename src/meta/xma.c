#include "meta.h"
#include "../coding/coding.h"

/* XMA - Microsoft format derived from RIFF, found in X360/XBone games */
VGMSTREAM * init_vgmstream_xma(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset, first_offset = 0xc;
    size_t data_size, chunk_size;
    int loop_flag, channel_count, sample_rate, is_xma2_old = 0, is_xma1 = 0;
    int num_samples, loop_start_sample, loop_end_sample, loop_start_b = 0, loop_end_b = 0, loop_subframe = 0;
    int fmt_be = 0;


    /* checks */
    /* .xma: standard
     * .xma2: Skullgirls (X360)
     * .wav: Super Meat Boy (X360)
     * .nps: Beautiful Katamari (X360)
     * .str: Sonic & Sega All Stars Racing (X360) */
    if ( !check_extensions(streamFile, "xma,xma2,wav,nps,str") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00, streamFile) != 0x52494646) /* "RIFF" */
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

        /* some Fable Heroes and Fable 3 XMA have a BE fmt chunk, but the rest of the file is still LE
         *  (incidentally they come with a "frsk" chunk) */
        if (format == 0x6601) { /* new XMA2 but BE */
            fmt_be = 1;
            format = 0x0166;
        }

        if (format == 0x165) { /* XMA1 */
            is_xma1 = 1;
            xma1_parse_fmt_chunk(streamFile, chunk_offset, &channel_count,&sample_rate, &loop_flag, &loop_start_b, &loop_end_b, &loop_subframe, fmt_be);
        } else if (format == 0x166) { /* new XMA2 */
            int32_t (*read_32bit)(off_t,STREAMFILE*) = fmt_be ? read_32bitBE : read_32bitLE;
            int16_t (*read_16bit)(off_t,STREAMFILE*) = fmt_be ? read_16bitBE : read_16bitLE;

            channel_count = read_16bit(chunk_offset+0x02,streamFile);
            sample_rate   = read_32bit(chunk_offset+0x04,streamFile);
            xma2_parse_fmt_chunk_extra(streamFile, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, fmt_be);
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


    /* get xma1 samples, later fixed */
    if (is_xma1) {
        ms_sample_data msd = {0};

        msd.xma_version = is_xma1 ? 1 : 2;
        msd.channels    = channel_count;
        msd.data_offset = start_offset;
        msd.data_size   = data_size;
        msd.loop_flag   = loop_flag;
        msd.loop_start_b= loop_start_b;
        msd.loop_end_b  = loop_end_b;
        msd.loop_start_subframe = loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */
        msd.chunk_offset= chunk_offset;

        xma_get_samples(&msd, streamFile);

        num_samples = msd.num_samples;
        loop_start_sample = msd.loop_start_sample;
        loop_end_sample = msd.loop_end_sample;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMA_RIFF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;

        if (is_xma2_old) {
            bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile);
        } else {
            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile, fmt_be);
        }

        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, streamFile, start_offset, data_size, chunk_offset, 1,1);
    }
#else
    goto fail;
#endif


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
 * Info only, not for playback as the encoder adjusts sample rate for looping purposes (sample<>data align),
 * When converting to PCM, xmaencode does use the modified sample rate.
 */
static int32_t get_xma_sample_rate(int32_t general_rate) {
    int32_t xma_rate = 48000; /* default XMA */

    if (general_rate <= 24000)      xma_rate = 24000;
    else if (general_rate <= 32000) xma_rate = 32000;
    else if (general_rate <= 44100) xma_rate = 44100;

    return xma_rate;
}
#endif
