#include "coding.h"

#ifdef VGM_USE_FFMPEG

static int ffmpeg_make_riff_atrac3(uint8_t* buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay) {
    uint16_t codec_ATRAC3 = 0x0270;
    size_t riff_size = 4+4+ 4 + 0x28 + 0x10 + 4+4;

    if (buf_size < riff_size)
        return -1;

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x20);/*fmt size*/
    put_16bitLE(buf+0x14, codec_ATRAC3);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_32bitLE(buf+0x20, (int16_t)(block_align)); /* block align */

    put_16bitLE(buf+0x24, 0x0e); /* extra data size */
    put_16bitLE(buf+0x26, 1); /* unknown, always 1 */
    put_16bitLE(buf+0x28, 0x0800 * channels); /* unknown (some size? 0x1000=2ch, 0x0800=1ch) */
    put_16bitLE(buf+0x2a, 0); /* unknown, always 0 */
    put_16bitLE(buf+0x2c, joint_stereo ? 0x0001 : 0x0000);
    put_16bitLE(buf+0x2e, joint_stereo ? 0x0001 : 0x0000); /* repeated? */
    put_16bitLE(buf+0x30, 1); /* unknown, always 1 (frame_factor?) */
    put_16bitLE(buf+0x32, 0); /* unknown, always 0 */

    memcpy(buf+0x34, "fact", 4);
    put_32bitLE(buf+0x38, 0x8); /* fact size */
    put_32bitLE(buf+0x3c, sample_count);
    put_32bitLE(buf+0x40, encoder_delay);

    memcpy(buf+0x44, "data", 4);
    put_32bitLE(buf+0x48, data_size); /* data size */

    return riff_size;
}

ffmpeg_codec_data* init_ffmpeg_atrac3_raw(STREAMFILE* sf, off_t offset, size_t data_size, int sample_count, int channels, int sample_rate, int block_align, int encoder_delay) {
    ffmpeg_codec_data *ffmpeg_data = NULL;
    uint8_t buf[0x100];
    int bytes;
    int joint_stereo = (block_align == 0x60*channels) && channels > 1; /* only lowest block size does joint stereo */
    int is_at3 = 1; /* could detect using block size */

    /* create fake header + init ffmpeg + apply fixes to FFmpeg decoding */
    bytes = ffmpeg_make_riff_atrac3(buf,sizeof(buf), sample_count, data_size, channels, sample_rate, block_align, joint_stereo, encoder_delay);
    ffmpeg_data = init_ffmpeg_header_offset(sf, buf,bytes, offset,data_size);
    if (!ffmpeg_data) goto fail;

    /* unlike with RIFF ATRAC3 we don't set implicit delay, as raw ATRAC3 headers often give loop/samples
     * in offsets, so calcs are expected to be handled externally (presumably the game would call raw decoding API
     * and any skips would be handled manually) */

    /* encoder delay: encoder introduces some garbage (not always silent) samples to skip at the beginning (at least 1 frame)
     * FFmpeg doesn't set this, and even if it ever does it's probably better to force it for the implicit skip. */
    ffmpeg_set_skip_samples(ffmpeg_data, encoder_delay);
    //ffmpeg_set_samples(sample_count); /* useful? */

    /* invert ATRAC3: waveform is inverted vs official tools (not noticeable but for accuracy) */
    if (is_at3) {
        ffmpeg_set_invert_floats(ffmpeg_data);
    }

    return ffmpeg_data;
fail:
    free_ffmpeg(ffmpeg_data);
    return NULL;
}

/* init ATRAC3/plus while adding some fixes */
ffmpeg_codec_data* init_ffmpeg_atrac3_riff(STREAMFILE* sf, off_t offset, int* p_samples) {
    ffmpeg_codec_data *ffmpeg_data = NULL;
    int is_at3 = 0, is_at3p = 0, codec;
    size_t riff_size;
    int fact_samples, skip_samples, implicit_skip;
    off_t fact_offset = 0;
    size_t fact_size = 0;


    /* some simplified checks just in case */
    if (read_32bitBE(offset + 0x00,sf) != 0x52494646) /* "RIFF" */
        goto fail;

    riff_size = read_32bitLE(offset + 0x04,sf) + 0x08;
    codec = (uint16_t)read_16bitLE(offset + 0x14, sf);
    switch(codec) {
        case 0x0270: is_at3 = 1; break;
        case 0xFFFE: is_at3p = 1; break;
        default: goto fail;
    }


    /* init ffmpeg + apply fixes to FFmpeg decoding (with these fixes should be
     * sample-accurate vs official tools, except usual +-1 float-to-pcm conversion) */
    ffmpeg_data = init_ffmpeg_offset(sf, offset, riff_size);
    if (!ffmpeg_data) goto fail;


    /* well behaved .at3 define "fact" but official tools accept files without it */
    if (find_chunk_le(sf,0x66616374,offset + 0x0c,0, &fact_offset, &fact_size)) { /* "fact" */
        if (fact_size == 0x08) { /* early AT3 (mainly PSP games) */
            fact_samples = read_32bitLE(fact_offset + 0x00, sf);
            skip_samples = read_32bitLE(fact_offset + 0x04, sf); /* base skip samples */
        }
        else if (fact_size == 0x0c) { /* late AT3 (mainly PS3 games and few PSP games) */
            fact_samples = read_32bitLE(fact_offset + 0x00, sf);
            /* 0x04: base skip samples, ignored by decoder */
            skip_samples = read_32bitLE(fact_offset + 0x08, sf); /* skip samples with implicit skip of 184 added */
        }
        else {
            VGM_LOG("ATRAC3: unknown fact size\n");
            goto fail;
        }
    }
    else {
        fact_samples = 0; /* tools output 0 samples in this case unless loop end is defined */
        if (is_at3)
            skip_samples = 1024; /* 1 frame */
        else if (is_at3p)
            skip_samples = 2048; /* 1 frame */
        else
            skip_samples = 0;
    }

    /* implicit skip: official tools skip this even with encoder delay forced to 0. Maybe FFmpeg decodes late,
     * but when forcing tools to decode all frame samples it always ends a bit before last frame, so maybe it's
     * really an internal skip, since encoder adds extra frames so fact num_samples + encoder delay + implicit skip
     * never goes past file. Same for all bitrate/channels, not added to loops. This is probably "decoder delay"
     * also seen in codecs like MP3 */
    if (is_at3) {
        implicit_skip = 69;
    }
    else if (is_at3p && fact_size == 0x08) {
        implicit_skip = 184*2;
    }
    else if (is_at3p && fact_size == 0x0c) {
        implicit_skip = 184; /* first 184 is already added to delay vs field at 0x08 */
    }
    else if (is_at3p) {
        implicit_skip = 184; /* default for unknown sizes */
    }
    else {
        implicit_skip = 0;
    }

    /* encoder delay: encoder introduces some garbage (not always silent) samples to skip at the beginning (at least 1 frame)
     * FFmpeg doesn't set this, and even if it ever does it's probably better to force it for the implicit skip. */
    ffmpeg_set_skip_samples(ffmpeg_data, skip_samples + implicit_skip);
    //ffmpeg_set_samples(sample_count); /* useful? */

    /* invert ATRAC3: waveform is inverted vs official tools (not noticeable but for accuracy) */
    if (is_at3) {
        ffmpeg_set_invert_floats(ffmpeg_data);
    }

    /* multichannel fix: LFE channel should be reordered on decode (ATRAC3Plus only, only 1/2/6/8ch exist):
     * - 6ch: FL FR FC BL BR LFE > FL FR FC LFE BL BR
     * - 8ch: FL FR FC BL BR SL SR LFE > FL FR FC LFE BL BR SL SR */
    if (is_at3p && ffmpeg_get_channels(ffmpeg_data) == 6) {
        /* LFE BR BL > LFE BL BR > same */
        int channel_remap[] = { 0, 1, 2, 5, 5, 5, };
        ffmpeg_set_channel_remapping(ffmpeg_data, channel_remap);
    }
    else if (is_at3p && ffmpeg_get_channels(ffmpeg_data) == 8) {
        /* LFE BR SL SR BL > LFE BL SL SR BR > LFE BL BR SR SL > LFE BL BR SL SR > same */
        int channel_remap[] = { 0, 1, 2, 7, 7, 7, 7, 7};
        ffmpeg_set_channel_remapping(ffmpeg_data, channel_remap);
    }


    if (p_samples)
        *p_samples = fact_samples;

    return ffmpeg_data;
fail:
    free_ffmpeg(ffmpeg_data);
    return NULL;
}

ffmpeg_codec_data* init_ffmpeg_aac(STREAMFILE* sf, off_t offset, size_t size, int skip_samples) {
    ffmpeg_codec_data* data = NULL;

    data = init_ffmpeg_offset(sf, offset, size);
    if (!data) goto fail;

    /* seeks to 0 eats first frame for whatever reason */
    ffmpeg_set_force_seek(data);

    /* raw AAC doesn't set this, while some decoders like FAAD remove 1024,
     * but should be handled in container as each encoder uses its own value
     * (Apple: 2112, FAAD: probably 1024, etc) */
    ffmpeg_set_skip_samples(data, skip_samples);

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

//TODO: make init_ffmpeg_xwma_fmt(be) too to pass fmt chunk

ffmpeg_codec_data* init_ffmpeg_xwma(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int format, int channels, int sample_rate, int avg_bitrate, int block_size) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;

    bytes = ffmpeg_make_riff_xwma(buf, sizeof(buf), format, data_size, channels, sample_rate, avg_bitrate, block_size);
    data = init_ffmpeg_header_offset(sf, buf,bytes, data_offset, data_size);
    if (!data) goto fail;

    if (format == 0x161) {
        int skip_samples = 0;

        /* Skip WMA encoder delay, not specified in the flags or containers (ASF/XWMA),
         * but verified compared to Microsoft's output. Seems to match frame_samples * 2 */
        if (sample_rate >= 32000)
            skip_samples = 4096;
        else if (sample_rate >= 22050)
            skip_samples = 2048;
        else if (sample_rate >= 8000)
            skip_samples = 1024;

        ffmpeg_set_skip_samples(data, skip_samples);
    }

    //TODO WMAPro uses variable skips and is more complex
    //TODO ffmpeg's WMA doesn't properly output trailing samples (ignored patch...)

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

#endif
