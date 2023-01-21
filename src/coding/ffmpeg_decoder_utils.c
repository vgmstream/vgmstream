#include "coding.h"

#ifdef VGM_USE_FFMPEG

/* Helper inits for FFmpeg's codecs. RIFF utils make headers for FFmpeg (it doesn't understand all valid RIFF headers,
 * nor the utils make 100% correct headers). */

static int ffmpeg_make_riff_atrac3(uint8_t* buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay) {

    int buf_max = (0x04 * 2 + 0x04) + (0x04 * 2 + 0x20) + (0x04 * 2 + 0x08) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2) + data_size); /* riff size */
    memcpy   (buf+0x08, "WAVE", 0x04);

    memcpy   (buf+0x0c, "fmt ", 0x04);
    put_u32le(buf+0x10, 0x20); /* fmt size */
    put_u16le(buf+0x14, 0x0270); /* ATRAC3 codec */
    put_u16le(buf+0x16, channels);
    put_u32le(buf+0x18, sample_rate);
    put_u32le(buf+0x1c, sample_rate * channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_u16le(buf+0x20, block_align); /* block align */

    put_u16le(buf+0x24, 0x0e); /* extra data size */
    put_u16le(buf+0x26, 1); /* unknown, always 1 */
    put_u16le(buf+0x28, 0x0800 * channels); /* unknown (some size? 0x1000=2ch, 0x0800=1ch) */
    put_u16le(buf+0x2a, 0); /* unknown, always 0 */
    put_u16le(buf+0x2c, joint_stereo ? 0x0001 : 0x0000);
    put_u16le(buf+0x2e, joint_stereo ? 0x0001 : 0x0000); /* repeated? */
    put_u16le(buf+0x30, 1); /* unknown, always 1 (frame_factor?) */
    put_u16le(buf+0x32, 0); /* unknown, always 0 */

    memcpy   (buf+0x34, "fact", 0x04);
    put_u32le(buf+0x38, 0x08); /* fact size */
    put_u32le(buf+0x3c, sample_count);
    put_u32le(buf+0x40, encoder_delay);

    memcpy   (buf+0x44, "data", 0x04);
    put_u32le(buf+0x48, data_size); /* data size */

    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_atrac3_raw(STREAMFILE* sf, off_t offset, size_t data_size, int sample_count, int channels, int sample_rate, int block_align, int encoder_delay) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;
    int joint_stereo = (block_align == 0x60*channels) && channels > 1; /* only lowest block size does joint stereo */
    int is_at3 = 1; /* could detect using block size */

    /* create fake header + init ffmpeg + apply fixes to FFmpeg decoding */
    bytes = ffmpeg_make_riff_atrac3(buf,sizeof(buf), sample_count, data_size, channels, sample_rate, block_align, joint_stereo, encoder_delay);
    data = init_ffmpeg_header_offset(sf, buf,bytes, offset,data_size);
    if (!data) goto fail;

    /* unlike with RIFF ATRAC3 we don't set implicit delay, as raw ATRAC3 headers often give loop/samples
     * in offsets, so calcs are expected to be handled externally (presumably the game would call raw decoding API
     * and any skips would be handled manually) */

    /* encoder delay: encoder introduces some garbage (not always silent) samples to skip at the beginning (at least 1 frame)
     * FFmpeg doesn't set this, and even if it ever does it's probably better to force it for the implicit skip. */
    ffmpeg_set_skip_samples(data, encoder_delay);
    //ffmpeg_set_samples(sample_count); /* useful? */

    /* invert ATRAC3: waveform is inverted vs official tools (not noticeable but for accuracy) */
    if (is_at3) {
        ffmpeg_set_invert_floats(data);
    }

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

/* init ATRAC3/plus while adding some fixes */
ffmpeg_codec_data* init_ffmpeg_atrac3_riff(STREAMFILE* sf, off_t offset, int* p_samples) {
    ffmpeg_codec_data* data = NULL;
    int is_at3 = 0, is_at3p = 0, codec;
    size_t riff_size;
    int fact_samples, skip_samples, implicit_skip;
    off_t fact_offset = 0;
    size_t fact_size = 0;


    /* some simplified checks just in case */
    if (!is_id32be(offset + 0x00,sf, "RIFF"))
        goto fail;

    riff_size = read_u32le(offset + 0x04,sf) + 0x08;
    codec = read_u16le(offset + 0x14, sf);
    switch(codec) {
        case 0x0270: is_at3 = 1; break;
        case 0xFFFE: is_at3p = 1; break;
        default: goto fail;
    }


    /* init ffmpeg + apply fixes to FFmpeg decoding (with these fixes should be
     * sample-accurate vs official tools, except usual +-1 float-to-pcm conversion) */
    data = init_ffmpeg_offset(sf, offset, riff_size);
    if (!data) goto fail;


    /* well behaved .at3 define "fact" but official tools accept files without it */
    if (find_chunk_le(sf, get_id32be("fact"), offset + 0x0c,0, &fact_offset, &fact_size)) {
        if (fact_size == 0x08) { /* early AT3 (mainly PSP games) */
            fact_samples = read_s32le(fact_offset + 0x00, sf);
            skip_samples = read_s32le(fact_offset + 0x04, sf); /* base skip samples */
        }
        else if (fact_size == 0x0c) { /* late AT3 (mainly PS3 games and few PSP games) */
            fact_samples = read_s32le(fact_offset + 0x00, sf);
            /* 0x04: base skip samples, ignored by decoder */
            skip_samples = read_s32le(fact_offset + 0x08, sf); /* skip samples with implicit skip of 184 added */
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
        implicit_skip = 184 * 2;
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
    ffmpeg_set_skip_samples(data, skip_samples + implicit_skip);
    //ffmpeg_set_samples(sample_count); /* useful? */

    /* invert ATRAC3: waveform is inverted vs official tools (not noticeable but for accuracy) */
    if (is_at3) {
        ffmpeg_set_invert_floats(data);
    }

    /* multichannel fix: LFE channel should be reordered on decode (ATRAC3Plus only, only 1/2/6/8ch exist):
     * - 6ch: FL FR FC BL BR LFE > FL FR FC LFE BL BR
     * - 8ch: FL FR FC BL BR SL SR LFE > FL FR FC LFE BL BR SL SR */
    if (is_at3p && ffmpeg_get_channels(data) == 6) {
        /* LFE BR BL > LFE BL BR > same */
        int channel_remap[] = { 0, 1, 2, 5, 5, 5, };
        ffmpeg_set_channel_remapping(data, channel_remap);
    }
    else if (is_at3p && ffmpeg_get_channels(data) == 8) {
        /* LFE BR SL SR BL > LFE BL SL SR BR > LFE BL BR SR SL > LFE BL BR SL SR > same */
        int channel_remap[] = { 0, 1, 2, 7, 7, 7, 7, 7};
        ffmpeg_set_channel_remapping(data, channel_remap);
    }


    if (p_samples)
        *p_samples = fact_samples;

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}


static int ffmpeg_make_riff_atrac3plus(uint8_t* buf, int buf_size, uint32_t data_size, int32_t sample_count, int channels, int sample_rate, int block_align, int encoder_delay) {

    int buf_max = (0x04 * 2 + 0x04) + (0x04 * 2 + 0x34) + (0x04 * 2 + 0x0c) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    /* standard */
    int channel_layout = 0;
    switch(channels) {
        case 1: channel_layout = mapping_MONO; break;
        case 2: channel_layout = mapping_STEREO; break;
        case 3: channel_layout = mapping_2POINT1; break;
        case 4: channel_layout = mapping_QUAD; break;
        case 5: channel_layout = mapping_5POINT0; break;
        case 6: channel_layout = mapping_5POINT1; break;
        default: break;
    }

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2) + data_size);
    memcpy   (buf+0x08, "WAVE", 0x04);

    memcpy   (buf+0x0c, "fmt ", 0x04);
    put_u32le(buf+0x10, 0x34);
    put_u16le(buf+0x14, 0xfffe); /* WAVEFORMATEXTENSIBLE */
    put_u16le(buf+0x16, channels);
    put_u32le(buf+0x18, sample_rate);
    put_u32le(buf+0x1c, sample_rate * channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_u32le(buf+0x20, block_align); /* block align */

    put_u16le(buf+0x24, 0x22); /* extra data size */
    put_u16le(buf+0x26, 0x0800); /* samples per block */
    put_u32le(buf+0x28, channel_layout);

    put_u32be(buf+0x2c, 0xBFAA23E9); /* GUID1 */
    put_u32be(buf+0x30, 0x58CB7144); /* GUID2 */
    put_u32be(buf+0x34, 0xA119FFFA); /* GUID3 */
    put_u32be(buf+0x38, 0x01E4CE62); /* GUID4 */
    put_u16be(buf+0x3c, 0x0010); /* unknown */
    put_u16be(buf+0x3e, 0x0000); /* unknown, atrac3plus config (varies with block size, FFmpeg doesn't use it) */
    put_u32be(buf+0x40, 0x00000000); /* reserved? */
    put_u32be(buf+0x44, 0x00000000); /* reserved? */

    memcpy   (buf+0x48, "fact", 0x04);
    put_u32le(buf+0x4c, 0x0c); /* fact size */
    put_u32le(buf+0x50, sample_count);
    put_u32le(buf+0x54, 0); /* unknown */
    put_u32le(buf+0x58, encoder_delay);

    memcpy   (buf+0x5c, "data", 0x04);
    put_u32le(buf+0x60, data_size); /* data size */

    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_atrac3plus_raw(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int32_t sample_count, int channels, int sample_rate, int block_align, int encoder_delay) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;

    bytes = ffmpeg_make_riff_atrac3plus(buf,sizeof(buf), data_size, sample_count, channels, sample_rate, block_align, encoder_delay);
    data = init_ffmpeg_header_offset(sf, buf, bytes, data_offset, data_size);
    if (!data) goto fail;

    ffmpeg_set_skip_samples(data, encoder_delay);

    return data;
fail:
    free_ffmpeg(data);
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


static int ffmpeg_make_riff_xwma(uint8_t* buf, size_t buf_size, uint32_t data_size, int format, int channels, int sample_rate, int avg_bps, int block_align) {
    int buf_max = (0x04 * 2 + 0x04) + (0x04 * 2 + 0x12) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    /* XWMA encoder only allows a few channel/sample rate/bitrate combinations,
     * but some create identical files with fake bitrate (1ch 22050hz at
     * 20/48/192kbps are all 20kbps, with the exact same codec data).
     * Decoder needs correct bitrate to work, so it's normalized here. */
    /* (may be removed once FFmpeg fixes this) */
    if (format == 0x161) { /* WMAv2 only */
        int ch = channels;
        int sr = sample_rate;
        int br = avg_bps * 8;

        /* Must be a bug in MS's encoder, as later versions of xWMAEncode remove these bitrates */
        if (ch == 1) { 
            if (sr == 22050 && (br==48000 || br==192000)) {
                br = 20000;
            }
            else if (sr == 32000 && (br==48000 || br==192000))
                br = 20000;
            else if (sr == 44100 && (br==96000 || br==192000))
                br = 48000;
        }
        else if (ch == 2) {
            if (sr == 22050 && (br==48000 || br==192000))
                br = 32000;
            else if (sr == 32000 && (br==192000))
                br = 48000;
        }

        avg_bps = br / 8;
    }

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2) + data_size); /* riff size */
    memcpy   (buf+0x08, "XWMA", 0x04);

    memcpy   (buf+0x0c, "fmt ", 0x04);
    put_u32le(buf+0x10, 0x12); /* fmt size */
    put_u16le(buf+0x14, format);
    put_u16le(buf+0x16, channels);
    put_u32le(buf+0x18, sample_rate);
    put_u32le(buf+0x1c, avg_bps); /* average bytes per second, somehow vital for XWMA */
    put_u16le(buf+0x20, block_align); /* block align */
    put_u16le(buf+0x22, 16); /* bits per sample */
    put_u16le(buf+0x24, 0); /* extra size */
    /* here goes the "dpds" seek table, but it's optional and not needed by FFmpeg (and also buggy) */

    memcpy   (buf+0x26, "data", 4);
    put_u32le(buf+0x2a, data_size);

    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_xwma(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int format, int channels, int sample_rate, int avg_bitrate, int block_size) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;

    bytes = ffmpeg_make_riff_xwma(buf, sizeof(buf), data_size, format, channels, sample_rate, avg_bitrate, block_size);
    data = init_ffmpeg_header_offset(sf, buf,bytes, data_offset, data_size);
    if (!data) goto fail;

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}


static int ffmpeg_make_riff_xma1(uint8_t* buf, size_t buf_size, size_t data_size, int channels, int sample_rate, int stream_mode) {

    /* stream disposition:
     * 0: default (ex. 5ch = 2ch + 2ch + 1ch = 3 streams)
     * 1: lineal (ex. 5ch = 1ch + 1ch + 1ch + 1ch + 1ch = 5 streams), unusual but exists
     * others: not seen (ex. maybe 5ch = 2ch + 1ch + 1ch + 1ch = 4 streams) */
    int streams;
    switch(stream_mode) {
        case 0 : streams = (channels + 1) / 2; break;
        case 1 : streams = channels; break;
        default: return 0;
    }

    int buf_max = (0x04 * 2 + 0x4) + (0x04 * 2 + 0x0c + 0x14 * streams) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2) + data_size); /* riff size */
    memcpy   (buf+0x08, "WAVE", 0x04);

    memcpy   (buf+0x0c, "fmt ", 0x04);
    put_u32le(buf+0x10, 0x0c + 0x14 * streams); /* fmt size */
    put_u16le(buf+0x14, 0x0165); /* XMA1 */
    put_u16le(buf+0x16, 16); /* bits per sample */
    put_u16le(buf+0x18, 0x10D6); /* encoder options */
    put_u16le(buf+0x1a, 0); /* largest stream skip (wrong, unneeded) */
    put_u16le(buf+0x1c, streams); /* number of streams */
    put_u8   (buf+0x1e, 0); /* loop count */
    put_u8   (buf+0x1f, 2); /* version */

    for (int i = 0; i < streams; i++) {
        int stream_channels;
        uint32_t speakers;
        off_t off = 0x20 + 0x14 * i; /* stream riff offset */

        if (stream_mode == 1) {
            /* lineal */
            stream_channels = 1;
            switch(i) { /* per stream, values observed */
                case 0: speakers = 0x0001; break;/* L */
                case 1: speakers = 0x0002; break;/* R */
                case 2: speakers = 0x0004; break;/* C */
                case 3: speakers = 0x0008; break;/* LFE */
                case 4: speakers = 0x0040; break;/* LB */
                case 5: speakers = 0x0080; break;/* RB */
                case 6: speakers = 0x0000; break;/* ? */
                case 7: speakers = 0x0000; break;/* ? */
                default: speakers = 0;
            }
        }
        else {
            /* with odd channels the last stream is mono */
            stream_channels = channels / streams + (channels%2 != 0 && i+1 != streams ? 1 : 0);
            switch(i) { /* per stream, values from xmaencode */
                case 0: speakers = stream_channels == 1 ? 0x0001 : 0x0201; break;/* L R */
                case 1: speakers = stream_channels == 1 ? 0x0004 : 0x0804; break;/* C LFE */
                case 2: speakers = stream_channels == 1 ? 0x0040 : 0x8040; break;/* LB RB */
                case 3: speakers = stream_channels == 1 ? 0x0000 : 0x0000; break;/* somehow empty (maybe should use 0x2010 LS RS) */
                default: speakers = 0;
            }
        }

        put_u32le(buf+off+0x00, sample_rate*stream_channels / sizeof(sample)); /* average bytes per second (wrong, unneeded) */
        put_u32le(buf+off+0x04, sample_rate);
        put_u32le(buf+off+0x08, 0); /* loop start */
        put_u32le(buf+off+0x0c, 0); /* loop end */
        put_u8   (buf+off+0x10, 0); /* loop subframe */
        put_u8   (buf+off+0x11, stream_channels);
        put_u16le(buf+off+0x12, speakers);
    }

    /* xmaencode decoding rejects XMA1 without "seek" chunk, though it doesn't seem to use it
     * (needs to be have entries but can be bogus, also generates seek for even small sounds) */

    memcpy   (buf + buf_max - (0x04 * 2), "data", 0x04);
    put_u32le(buf + buf_max - (0x04 * 1), data_size);

    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_xma1_raw(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int channels, int sample_rate, int stream_mode) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;

    bytes = ffmpeg_make_riff_xma1(buf, sizeof(buf), data_size, channels, sample_rate, stream_mode);
    data = init_ffmpeg_header_offset(sf, buf, bytes, data_offset, data_size);
    if (!data) goto fail;

    /* n5.1.2 XMA1 hangs on seeks near end (infinite loop), presumably due to missing flush in wmapro.c's ff_xma1_decoder + frame skip samples */
    ffmpeg_set_force_seek(data);

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}


static int ffmpeg_make_riff_xma2(uint8_t* buf, size_t buf_size, size_t data_size, int32_t sample_count, int channels, int sample_rate, int block_size, int block_count) {
    size_t bytecount;
    int streams;
    uint32_t speakers;

    int buf_max = (0x04 * 2 + 0x04) + (0x04 * 2 + 0x34) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    /* info from xma2defs.h, xact3wb.h and audiodefs.h */
    streams = (channels + 1) / 2;
    switch (channels) {
        case 1: speakers = 0x04; break; /* 1.0: FC */
        case 2: speakers = 0x01 | 0x02; break; /* 2.0: FL FR */
        case 3: speakers = 0x01 | 0x02 | 0x08; break; /* 2.1: FL FR LF */
        case 4: speakers = 0x01 | 0x02 | 0x10 | 0x20; break; /* 4.0: FL FR BL BR */
        case 5: speakers = 0x01 | 0x02 | 0x08 | 0x10 | 0x20; break; /* 4.1: FL FR LF BL BR */
        case 6: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20; break; /* 5.1: FL FR FC LF BL BR */
        case 7: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x100; break; /* 6.1: FL FR FC LF BL BR BC */
        case 8: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80; break; /* 7.1: FL FR FC LF BL BR FLC FRC */
        default: speakers = 0; break;
    }

    bytecount = sample_count * channels * sizeof(sample);

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2) + data_size); /* riff size */
    memcpy   (buf+0x08, "WAVE", 0x04);

    memcpy   (buf+0x0c, "fmt ", 0x04);
    put_u32le(buf+0x10, 0x34); /* fmt size */
    put_u16le(buf+0x14, 0x0166); /* XMA2 */
    put_u16le(buf+0x16, channels);
    put_u32le(buf+0x18, sample_rate);
    put_u32le(buf+0x1c, sample_rate * channels / sizeof(sample)); /* average bytes per second (wrong, unneeded) */
    put_u16le(buf+0x20, (uint16_t)(channels * sizeof(sample))); /* block align */
    put_u16le(buf+0x22, 16); /* bits per sample */

    put_u16le(buf+0x24, 0x22); /* extra data size */
    put_u16le(buf+0x26, streams); /* number of streams */
    put_u32le(buf+0x28, speakers); /* speaker position  */
    put_u32le(buf+0x2c, bytecount); /* PCM samples */
    put_u32le(buf+0x30, block_size); /* XMA block size (can be zero, for seeking only) */
    /* (looping values not set, expected to be handled externally) */
    put_u32le(buf+0x34, 0); /* play begin */
    put_u32le(buf+0x38, 0); /* play length */
    put_u32le(buf+0x3c, 0); /* loop begin */
    put_u32le(buf+0x40, 0); /* loop length */
    put_u8   (buf+0x44, 0); /* loop count */
    put_u8   (buf+0x45, 4); /* encoder version */
    put_u16le(buf+0x46, block_count); /* blocks count (entries in seek table, can be zero) */

    memcpy   (buf+0x48, "data", 0x04);
    put_u32le(buf+0x4c, data_size); /* data size */

    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_xma2_raw(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int32_t sample_count, int channels, int sample_rate, int block_size, int block_count) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int bytes;

    /* seemingly not needed but just in case */
    if (block_size <= 0)
        block_size = 0x8000; /* default */
    if (block_count <= 0)
        block_count = (data_size / block_size) + (data_size % block_size != 0 ? 1 : 0); /* approx */

    bytes = ffmpeg_make_riff_xma2(buf, sizeof(buf), data_size, sample_count, channels, sample_rate, block_size, block_count);
    data = init_ffmpeg_header_offset(sf, buf, bytes, data_offset, data_size);
    if (!data) goto fail;

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}


/* swap from LE to BE or the other way around */
static int ffmpeg_fmt_chunk_swap_endian(uint8_t* chunk, uint32_t chunk_size, uint16_t codec) {
    int i;

    switch(codec) {
        case 0x6501: 
        case 0x0165: /* XMA1 */
            put_u16le(chunk + 0x00, get_u16be(chunk + 0x00)); /*FormatTag*/
            put_u16le(chunk + 0x02, get_u16be(chunk + 0x02)); /*BitsPerSample*/
            put_u16le(chunk + 0x04, get_u16be(chunk + 0x04)); /*EncodeOptions*/
            put_u16le(chunk + 0x06, get_u16be(chunk + 0x06)); /*LargestSkip*/
            put_u16le(chunk + 0x08, get_u16be(chunk + 0x08)); /*NumStreams*/
            // put_u8(chunk + 0x0a,    get_u8(chunk + 0x0a)); /*LoopCount*/
            // put_u8(chunk + 0x0b,    get_u8(chunk + 0x0b)); /*Version*/
            for (i = 0xc; i < chunk_size; i += 0x14) { /* reverse endianness for each stream */
                put_u32le(chunk + i + 0x00, get_u32be(chunk + i + 0x00)); /*PsuedoBytesPerSec*/
                put_u32le(chunk + i + 0x04, get_u32be(chunk + i + 0x04)); /*SampleRate*/
                put_u32le(chunk + i + 0x08, get_u32be(chunk + i + 0x08)); /*LoopStart*/
                put_u32le(chunk + i + 0x0c, get_u32be(chunk + i + 0x0c)); /*LoopEnd*/
                // put_u8(chunk + i + 0x10,    get_u8(chunk + i + 0x10)); /*SubframeData*/
                // put_u8(chunk + i + 0x11,    get_u8(chunk + i + 0x11)); /*Channels*/
                put_u16le(chunk + i + 0x12, get_u16be(chunk + i + 0x12)); /*ChannelMask*/
            }
            break;

        case 0x6601:
        case 0x0166: /* XMA2 */
            put_u16le(chunk + 0x00, get_u16be(chunk + 0x00)); /*wFormatTag*/
            put_u16le(chunk + 0x02, get_u16be(chunk + 0x02)); /*nChannels*/
            put_u32le(chunk + 0x04, get_u32be(chunk + 0x04)); /*nSamplesPerSec*/
            put_u32le(chunk + 0x08, get_u32be(chunk + 0x08)); /*nAvgBytesPerSec*/
            put_u16le(chunk + 0x0c, get_u16be(chunk + 0x0c)); /*nBlockAlign*/
            put_u16le(chunk + 0x0e, get_u16be(chunk + 0x0e)); /*wBitsPerSample*/
            put_u16le(chunk + 0x10, get_u16be(chunk + 0x10)); /*cbSize*/
            put_u16le(chunk + 0x12, get_u16be(chunk + 0x12)); /*NumStreams*/
            put_u32le(chunk + 0x14, get_u32be(chunk + 0x14)); /*ChannelMask*/
            put_u32le(chunk + 0x18, get_u32be(chunk + 0x18)); /*SamplesEncoded*/
            put_u32le(chunk + 0x1c, get_u32be(chunk + 0x1c)); /*BytesPerBlock*/
            put_u32le(chunk + 0x20, get_u32be(chunk + 0x20)); /*PlayBegin*/
            put_u32le(chunk + 0x24, get_u32be(chunk + 0x24)); /*PlayLength*/
            put_u32le(chunk + 0x28, get_u32be(chunk + 0x28)); /*LoopBegin*/
            put_u32le(chunk + 0x2c, get_u32be(chunk + 0x2c)); /*LoopLength*/
            // put_u8(chunk + 0x30,    get_u8(chunk + 0x30)); /*LoopCount*/
            // put_u8(chunk + 0x31,    get_u8(chunk + 0x31)); /*EncoderVersion*/
            put_u16le(chunk + 0x32, get_u16be(chunk + 0x32)); /*BlockCount*/
            break;

        default:
            goto fail;
    }

    return 1;
fail:
    return 0;
}


/* Makes a XMA1/2 RIFF header using a "fmt " chunk (XMAWAVEFORMAT/XMA2WAVEFORMATEX) or "XMA2" chunk (XMA2WAVEFORMAT), as a base:
 * Useful to preserve the stream layout */
static int ffmpeg_make_riff_xma_chunk(STREAMFILE* sf, uint8_t* buf, int buf_size, uint32_t data_size, uint32_t chunk_offset, uint32_t chunk_size, int* p_is_xma1) {
    int buf_max = (0x04 * 2 + 0x04) + (0x04 * 2 + chunk_size) + (0x04 * 2);
    if (buf_max > buf_size)
        return 0;

    if (read_streamfile(buf+0x14, chunk_offset, chunk_size, sf) != chunk_size)
        return 0;

    /* checks info from the chunk itself */
    int is_xma1 = 0;
    int is_xma2_old = buf[0x14] == 0x03 || buf[0x14] == 0x04;
    if (!is_xma2_old) {
        uint16_t codec = get_u16le(buf+0x14);
        int is_be = (codec > 0x1000);
        if (is_be)
            ffmpeg_fmt_chunk_swap_endian(buf+0x14, chunk_size, codec);
        is_xma1 = codec == 0x0165 || codec == 0x6501;
    }

    memcpy   (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, buf_max - (0x04 * 2)+ data_size); /* riff size */
    memcpy   (buf+0x08, "WAVE", 0x04);
    memcpy   (buf+0x0c, is_xma2_old ? "XMA2" : "fmt ", 0x04);
    put_u32le(buf+0x10, chunk_size);
    /* copied chunk in between */
    memcpy   (buf+0x14 + chunk_size + 0x00, "data", 0x04);
    put_u32le(buf+0x14 + chunk_size + 0x04, data_size);

    *p_is_xma1 = is_xma1;
    return buf_max;
}

ffmpeg_codec_data* init_ffmpeg_xma_chunk(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, uint32_t chunk_offset, uint32_t chunk_size) {
    return init_ffmpeg_xma_chunk_split(sf, sf, data_offset, data_size, chunk_offset, chunk_size);
}

ffmpeg_codec_data* init_ffmpeg_xma_chunk_split(STREAMFILE* sf_head, STREAMFILE* sf_data, uint32_t data_offset, uint32_t data_size, uint32_t chunk_offset, uint32_t chunk_size) {
    ffmpeg_codec_data* data = NULL;
    uint8_t buf[0x100];
    int is_xma1 = 0;

    int bytes = ffmpeg_make_riff_xma_chunk(sf_head, buf, sizeof(buf), data_size, chunk_offset, chunk_size, &is_xma1);
    data = init_ffmpeg_header_offset(sf_data, buf, bytes, data_offset, data_size);
    if (!data) goto fail;

    /* n5.1.2 XMA1 hangs on seeks near end (infinite loop), presumably due to missing flush in wmapro.c's ff_xma1_decoder + frame skip samples */
    if (is_xma1)
        ffmpeg_set_force_seek(data);

    return data;
fail:
    free_ffmpeg(data);
    return NULL;
}

#endif
