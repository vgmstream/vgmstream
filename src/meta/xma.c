#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "../util/endianness.h"


/* XMA - Microsoft format derived from RIFF, found in X360/XBone games */
VGMSTREAM* init_vgmstream_xma(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset = 0, data_size = 0, chunk_offset = 0, chunk_size = 0;
    int loop_flag, channels, sample_rate, is_xma2_new = 0, is_xma2_old = 0, is_xma1 = 0;
    int32_t num_samples, loop_start_sample, loop_end_sample, loop_start_b = 0, loop_end_b = 0, loop_subframe = 0;
    int fmt_be = 0;


    /* checks */
    if (!is_id32be(0x00, sf, "RIFF"))
        goto fail;

    /* .xma: standard
     * .xma2: Skullgirls (X360)
     * .wav: Super Meat Boy (X360)
     * .nps: Beautiful Katamari (X360)
     * .str: Sonic & Sega All Stars Racing (X360)
     * .kmx: Warriors: Legends of Troy (X360) */
    if (!check_extensions(sf, "xma,xma2,wav,lwav,nps,str,kmx"))
        goto fail;

    {
        uint32_t file_size = get_streamfile_size(sf);
        uint32_t riff_size = read_u32le(0x04,sf);

        /* some Beautiful Katamari set same riff/file size (those with "XMA2" and without "fmt ") */
        if (riff_size != file_size && riff_size + 0x08 > file_size)
            goto fail;
    }


    /* parse LE RIFF header, with "XMA2" (XMA2WAVEFORMAT) or "fmt " (XMAWAVEFORMAT/XMA2WAVEFORMAT) main chunks
     * Often comes with an optional "seek" chunk too */

    /* parse chunks (reads once linearly, as XMA2 chunk often goes near EOF) */
    {
        chunk_t rc = {0};

        /* chunks are even-aligned and don't need to add padding byte, unlike real RIFFs */
        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {

            switch(rc.type) {
                case 0x584D4132: /* "XMA2" */
                    chunk_offset = rc.offset;
                    chunk_size = rc.size;

                    is_xma2_old = 1;
                    xma2_parse_xma2_chunk(sf, chunk_offset, &channels,&sample_rate, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample);
                    break;

                case 0x666d7420: { /* "fmt " */
                    int format;
                    if (is_xma2_old) break; /* fmt has XMA1 info, favor XMA2 chunk */

                    chunk_offset = rc.offset;
                    chunk_size = rc.size;

                    format = read_u16le(rc.offset + 0x00,sf);

                    /* some Fable Heroes and Fable 3 XMA have a BE fmt chunk, but the rest of the file is still LE
                    *  (incidentally they come with a "frsk" chunk) */
                    if (format == 0x6601) { /* new XMA2 but BE */
                        fmt_be = 1;
                        format = 0x0166;
                    }

                    if (format == 0x165) { /* XMA1 */
                        is_xma1 = 1;
                        xma1_parse_fmt_chunk(sf, chunk_offset, &channels,&sample_rate, &loop_flag, &loop_start_b, &loop_end_b, &loop_subframe, fmt_be);
                    }
                    else if (format == 0x166) { /* new XMA2 */
                        read_s32_t read_s32 = fmt_be ? read_s32be : read_s32le;
                        read_u16_t read_u16 = fmt_be ? read_u16be : read_u16le;

                        is_xma2_new = 1;
                        channels    = read_u16(chunk_offset + 0x02,sf);
                        sample_rate = read_s32(chunk_offset + 0x04,sf);
                        xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, fmt_be);
                    }
                    else {
                        goto fail;
                    }
                    break;
                }

                case 0x64617461: /* "data" */
                    start_offset = rc.offset;
                    data_size = rc.size;
                    break;

                default: /* others: "seek", "ALGN" */
                    break;
            }
        }

        if (!is_xma2_new && !is_xma2_old && !is_xma1)
            goto fail;
    }


    /* get xma1 samples, later fixed */
    if (!is_xma2_old && !is_xma2_new && is_xma1) {
        ms_sample_data msd = {0};

        msd.xma_version = is_xma1 ? 1 : 2;
        msd.channels    = channels;
        msd.data_offset = start_offset;
        msd.data_size   = data_size;
        msd.loop_flag   = loop_flag;
        msd.loop_start_b= loop_start_b;
        msd.loop_end_b  = loop_end_b;
        msd.loop_start_subframe = loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */
        msd.chunk_offset = chunk_offset;

        xma_get_samples(&msd, sf);

        num_samples = msd.num_samples;
        loop_start_sample = msd.loop_start_sample;
        loop_end_sample = msd.loop_end_sample;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMA_RIFF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1);
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
