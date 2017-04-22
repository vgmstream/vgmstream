#include "coding.h"
#include "math.h"
#include "../vgmstream.h"


/**
 * Various utils for formats that aren't handled their own decoder or meta
 *
 * ffmpeg_make_riff_* utils don't depend on FFmpeg, but rather, they make headers that FFmpeg
 * can use (it doesn't understand all valid RIFF headers, nor the utils make 100% correct headers).
 */


/* ******************************************** */
/* INTERNAL UTILS                               */
/* ******************************************** */

/**
 * read num_bits (up to 25) from a bit offset.
 * 25 since we read a 32 bit int, and need to adjust up to 7 bits from the byte-rounded fseek (32-7=25)
 */
static uint32_t read_bitsBE_b(off_t bit_offset, int num_bits, STREAMFILE *streamFile) {
    uint32_t num, mask;
    if (num_bits > 25) return -1; //???

    num = read_32bitBE(bit_offset / 8, streamFile); /* fseek rounded to 8 */
    num = num << (bit_offset % 8); /* offset adjust (up to 7) */
    num = num >> (32 - num_bits);
    mask = 0xffffffff >> (32 - num_bits);

    return num & mask;
}


/* ******************************************** */
/* FAKE RIFF HELPERS                            */
/* ******************************************** */
/* All helpers copy a RIFF header to buf and returns the number of bytes in buf or -1 when buf is not big enough */

int ffmpeg_make_riff_atrac3(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay) {
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

int ffmpeg_make_riff_atrac3plus(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int encoder_delay) {
    uint16_t codec_ATRAC3plus = 0xfffe; /* wave format extensible */
    size_t riff_size = 4+4+ 4 + 0x3c + 0x14 + 4+4;

    if (buf_size < riff_size)
        return -1;

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x34);/*fmt size*/
    put_16bitLE(buf+0x14, codec_ATRAC3plus);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels / sizeof(sample)); /* average bytes per second (wrong) */
    put_32bitLE(buf+0x20, (int16_t)(block_align)); /* block align */

    put_16bitLE(buf+0x24, 0x22); /* extra data size */
    put_16bitLE(buf+0x26, 0x0800); /* samples per block */
    put_32bitLE(buf+0x28, 0x0000003); /* unknown */
    put_32bitBE(buf+0x2c, 0xBFAA23E9); /* GUID1 */
    put_32bitBE(buf+0x30, 0x58CB7144); /* GUID2 */
    put_32bitBE(buf+0x34, 0xA119FFFA); /* GUID3 */
    put_32bitBE(buf+0x38, 0x01E4CE62); /* GUID4 */
    put_16bitBE(buf+0x3c, 0x0010); /* unknown */
    put_16bitBE(buf+0x3e, 0x0000); /* config */ //todo this varies with block size, but FFmpeg doesn't use it
    put_32bitBE(buf+0x40, 0x00000000); /* empty */
    put_32bitBE(buf+0x44, 0x00000000); /* empty */

    memcpy(buf+0x48, "fact", 4);
    put_32bitLE(buf+0x4c, 0x0c); /* fact size */
    put_32bitLE(buf+0x50, sample_count);
    put_32bitLE(buf+0x54, 0); /* unknown */
    put_32bitLE(buf+0x58, encoder_delay);

    memcpy(buf+0x5c, "data", 4);
    put_32bitLE(buf+0x60, data_size); /* data size */

    return riff_size;
}

int ffmpeg_make_riff_xma1(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int stream_mode) {
    uint16_t codec_XMA1 = 0x0165;
    size_t riff_size;
    int streams, i;

    /* stream disposition:
     * 0: default (ex. 5ch = 2ch + 2ch + 1ch = 3 streams)
     * 1: lineal (ex. 5ch = 1ch + 1ch + 1ch + 1ch + 1ch = 5 streams), unusual but exists
     * others: not seen (ex. maybe 5ch = 2ch + 1ch + 1ch + 1ch = 4 streams) */
    switch(stream_mode) {
        case 0 : streams = (channels + 1) / 2; break;
        case 1 : streams = channels; break;
        default: return 0;
    }

    riff_size = 4+4+ 4 + 0x14 + 0x14*streams + 4+4;

    if (buf_size < riff_size)
        return -1;

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0xc + 0x14*streams);/*fmt size*/
    put_16bitLE(buf+0x14, codec_XMA1);
    put_16bitLE(buf+0x16, 16); /* bits per sample */
    put_16bitLE(buf+0x18, 0x10D6); /* encoder options */
    put_16bitLE(buf+0x1a, 0); /* largest stream skip (wrong, unneeded) */
    put_16bitLE(buf+0x1c, streams); /* number of streams */
    put_8bit   (buf+0x1e, 0); /* loop count */
    put_8bit   (buf+0x1f, 2); /* version */

    for (i = 0; i < streams; i++) {
        int stream_channels;
        uint32_t speakers;
        off_t off = 0x20 + 0x14*i;/* stream riff offset */

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

        put_32bitLE(buf+off+0x00, sample_rate*stream_channels / sizeof(sample)); /* average bytes per second (wrong, unneeded) */
        put_32bitLE(buf+off+0x04, sample_rate);
        put_32bitLE(buf+off+0x08, 0); /* loop start */
        put_32bitLE(buf+off+0x0c, 0); /* loop end */
        put_8bit   (buf+off+0x10, 0); /* loop subframe */
        put_8bit   (buf+off+0x11, channels);
        put_16bitLE(buf+off+0x12, speakers);
    }

    memcpy(buf+riff_size-4-4, "data", 4);
    put_32bitLE(buf+riff_size-4, data_size); /* data size */

    return riff_size;
}

int ffmpeg_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size) {
    uint16_t codec_XMA2 = 0x0166;
    size_t riff_size = 4+4+ 4 + 0x3c + 4+4;
    size_t bytecount;
    int streams;
    uint32_t speakers;

    /* info from xma2defs.h, xact3wb.h and audiodefs.h */
    streams = (channels + 1) / 2;
    switch (channels) {
        case 1: speakers = 0x04; break; /* 1.0: FC */
        case 2: speakers = 0x01 | 0x02; break; /* 2.0: FL FR */
        case 3: speakers = 0x01 | 0x02 | 0x08; break; /* 2.1: FL FR LF */
        case 4: speakers = 0x01 | 0x02 | 0x10 | 0x20; break; /* 4.0: FL FR BL BR */
        case 5: speakers = 0x01 | 0x02 | 0x08 | 0x10 | 0x20; break; /* 4.1: FL FR LF BL BR */
        case 6: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20; break; /* 5.1: FL FR FC LF BL BR */
        case 7: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x0100; break; /* 6.1: FL FR FC LF BL BR BC */
        case 8: speakers = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80; break; /* 7.1: FL FR FC LF BL BR FLC FRC */
        default: speakers = 0; break;
    }

    if (buf_size < riff_size)
        return -1;

    bytecount = sample_count * channels * sizeof(sample);

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x34);/*fmt size*/
    put_16bitLE(buf+0x14, codec_XMA2);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels / sizeof(sample)); /* average bytes per second (wrong unneeded) */
    put_16bitLE(buf+0x20, (int16_t)(channels*sizeof(sample))); /* block align */
    put_16bitLE(buf+0x22, 16); /* bits per sample */

    put_16bitLE(buf+0x24, 0x22); /* extra data size */
    put_16bitLE(buf+0x26, streams); /* number of streams */
    put_32bitLE(buf+0x28, speakers); /* speaker position  */
    put_32bitLE(buf+0x2c, bytecount); /* PCM samples */
    put_32bitLE(buf+0x30, block_size); /* XMA block size */
    /* (looping values not set, expected to be handled externally) */
    put_32bitLE(buf+0x34, 0); /* play begin */
    put_32bitLE(buf+0x38, 0); /* play length */
    put_32bitLE(buf+0x3c, 0); /* loop begin */
    put_32bitLE(buf+0x40, 0); /* loop length */
    put_8bit(buf+0x44, 0); /* loop count */
    put_8bit(buf+0x45, 4); /* encoder version */
    put_16bitLE(buf+0x46, block_count); /* blocks count = entries in seek table */

    memcpy(buf+0x48, "data", 4);
    put_32bitLE(buf+0x4c, data_size); /* data size */

    return riff_size;
}

/* Makes a XMA1/2 RIFF header for FFmpeg using a "fmt " chunk (XMAWAVEFORMAT or XMA2WAVEFORMATEX) as a base:
 * Useful to preserve the stream layout */
int ffmpeg_make_riff_xma_from_fmt_chunk(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian) {
    size_t riff_size = 4+4+ 4 + 4+4+fmt_size + 4+4;
    uint8_t chunk[0x100];

    if (buf_size < riff_size || fmt_size > 0x100)
        goto fail;
    if (read_streamfile(chunk,fmt_offset,fmt_size, streamFile) != fmt_size)
        goto fail;

    if (big_endian) {
        int codec = read_16bitBE(fmt_offset,streamFile);
        ffmpeg_fmt_chunk_swap_endian(chunk, fmt_size, codec);
    }

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, fmt_size);/*fmt size*/
    memcpy(buf+0x14, chunk, fmt_size);

    memcpy(buf+0x14+fmt_size, "data", 4);
    put_32bitLE(buf+0x14+fmt_size+4, data_size); /* data size */

    return riff_size;

fail:
    return -1;
}

/* Makes a XMA2 RIFF header for FFmpeg using a "XMA2" chunk (XMA2WAVEFORMAT) as a base.
 * Useful to preserve the stream layout */
int ffmpeg_make_riff_xma2_from_xma2_chunk(uint8_t * buf, size_t buf_size, off_t xma2_offset, size_t xma2_size, size_t data_size, STREAMFILE *streamFile) {
    uint8_t chunk[0x100];
    size_t riff_size;
    size_t xma2_final_size = xma2_size;
    int xma2_chunk_version = read_8bit(xma2_offset,streamFile);

    /* FFmpeg can't parse v3 "XMA2" chunks so we'll have to extend (8 bytes in the middle) */
    if (xma2_chunk_version == 3)
        xma2_final_size += 0x8;
    riff_size = 4+4+ 4 + 4+4+xma2_final_size + 4+4;

    if (buf_size < riff_size || xma2_final_size > 0x100)
        goto fail;
    if (read_streamfile(chunk,xma2_offset,xma2_size, streamFile) != xma2_size)
        goto fail;


    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "XMA2", 4);
    put_32bitLE(buf+0x10, xma2_final_size);
    if (xma2_chunk_version == 3) {
        /* old XMA2 v3: change to v4 (extra 8 bytes in the middle); always BE */
        put_8bit   (buf+0x14 + 0x00, 4); /* v4 */
        memcpy     (buf+0x14 + 0x01, chunk+1, 0xF); /* first v3 part (fixed) */
        put_32bitBE(buf+0x14 + 0x10, 0x000010D6); /* extra v4 BE: "EncodeOptions" (not used by FFmpeg) */
        put_32bitBE(buf+0x14 + 0x14, 0); /* extra v4 BE: "PsuedoBytesPerSec" (not used by FFmpeg) */
        memcpy     (buf+0x14 + 0x18, chunk+0x10, xma2_size - 0x10); /* second v3 part (variable size) */
    } else {
        memcpy(buf+0x14, chunk, xma2_size);
    }

    memcpy(buf+0x14+xma2_final_size, "data", 4);
    put_32bitLE(buf+0x14+xma2_final_size+4, data_size); /* data size */

    return riff_size;

fail:
    return -1;
}

int ffmpeg_make_riff_xwma(uint8_t * buf, size_t buf_size, int codec, size_t data_size, int channels, int sample_rate, int avg_bps, int block_align) {
    size_t riff_size = 4+4+ 4 + 0x1a + 4+4;

    if (buf_size < riff_size)
        return -1;

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "XWMA", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x12);/*fmt size*/
    put_16bitLE(buf+0x14, codec);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, avg_bps); /* average bytes per second, somehow vital for XWMA */
    put_16bitLE(buf+0x20, block_align); /* block align */
    put_16bitLE(buf+0x22, 16); /* bits per sample */
    put_16bitLE(buf+0x24, 0); /* extra size */
    /* here goes the "dpds" table, but it's optional and not needed by FFmpeg */

    memcpy(buf+0x26, "data", 4);
    put_32bitLE(buf+0x2a, data_size); /* data size */

    return riff_size;
}


int ffmpeg_fmt_chunk_swap_endian(uint8_t * chunk, size_t chunk_size, uint16_t codec) {
    int i;
    /* swap from LE to BE or the other way around, doesn't matter */
    switch(codec) {
        case 0x165: { /* XMA1 */
            put_16bitLE(chunk + 0x00, get_16bitBE(chunk + 0x00));/*FormatTag*/
            put_16bitLE(chunk + 0x02, get_16bitBE(chunk + 0x02));/*BitsPerSample*/
            put_16bitLE(chunk + 0x04, get_16bitBE(chunk + 0x04));/*EncodeOptions*/
            put_16bitLE(chunk + 0x06, get_16bitBE(chunk + 0x06));/*LargestSkip*/
            put_16bitLE(chunk + 0x08, get_16bitBE(chunk + 0x08));/*NumStreams*/
            // put_8bit(chunk + 0x0a,    get_8bit(chunk + 0x0a));/*LoopCount*/
            // put_8bit(chunk + 0x0b,    get_8bit(chunk + 0x0b));/*Version*/
            for (i = 0xc; i < chunk_size; i += 0x14) { /* reverse endianness for each stream */
                put_32bitLE(chunk + i + 0x00, get_32bitBE(chunk + i + 0x00));/*PsuedoBytesPerSec*/
                put_32bitLE(chunk + i + 0x04, get_32bitBE(chunk + i + 0x04));/*SampleRate*/
                put_32bitLE(chunk + i + 0x08, get_32bitBE(chunk + i + 0x08));/*LoopStart*/
                put_32bitLE(chunk + i + 0x0c, get_32bitBE(chunk + i + 0x0c));/*LoopEnd*/
                // put_8bit(chunk + i + 0x10,    get_8bit(chunk + i + 0x10));/*SubframeData*/
                // put_8bit(chunk + i + 0x11,    get_8bit(chunk + i + 0x11));/*Channels*/
                put_16bitLE(chunk + i + 0x12, get_16bitBE(chunk + i + 0x12));/*ChannelMask*/
            }
            break;
        }

        case 0x166: { /* XMA2 */
            put_16bitLE(chunk + 0x00, get_16bitBE(chunk + 0x00));/*wFormatTag*/
            put_16bitLE(chunk + 0x02, get_16bitBE(chunk + 0x02));/*nChannels*/
            put_32bitLE(chunk + 0x04, get_32bitBE(chunk + 0x04));/*nSamplesPerSec*/
            put_32bitLE(chunk + 0x08, get_32bitBE(chunk + 0x08));/*nAvgBytesPerSec*/
            put_16bitLE(chunk + 0x0c, get_16bitBE(chunk + 0x0c));/*nBlockAlign*/
            put_16bitLE(chunk + 0x0e, get_16bitBE(chunk + 0x0e));/*wBitsPerSample*/
            put_16bitLE(chunk + 0x10, get_16bitBE(chunk + 0x10));/*cbSize*/
            put_16bitLE(chunk + 0x12, get_16bitBE(chunk + 0x12));/*NumStreams*/
            put_32bitLE(chunk + 0x14, get_32bitBE(chunk + 0x14));/*ChannelMask*/
            put_32bitLE(chunk + 0x18, get_32bitBE(chunk + 0x18));/*SamplesEncoded*/
            put_32bitLE(chunk + 0x1c, get_32bitBE(chunk + 0x1c));/*BytesPerBlock*/
            put_32bitLE(chunk + 0x20, get_32bitBE(chunk + 0x20));/*PlayBegin*/
            put_32bitLE(chunk + 0x24, get_32bitBE(chunk + 0x24));/*PlayLength*/
            put_32bitLE(chunk + 0x28, get_32bitBE(chunk + 0x28));/*LoopBegin*/
            put_32bitLE(chunk + 0x2c, get_32bitBE(chunk + 0x2c));/*LoopLength*/
            /* put_8bit(chunk + 0x30,    get_8bit(chunk + 0x30));*//*LoopCount*/
            /* put_8bit(chunk + 0x31,    get_8bit(chunk + 0x31));*//*EncoderVersion*/
            put_16bitLE(chunk + 0x32, get_16bitBE(chunk + 0x32));/*BlockCount*/
            break;
        }
        default:
            goto fail;
    }

    return 1;

fail:
    return 0;
}


/* ******************************************** */
/* XMA PARSING                                  */
/* ******************************************** */
#define XMA_CHECK_SKIPS                 0

/**
 * Find total and loop samples of Microsoft audio formats (WMAPRO/XMA1/XMA2) by reading frame headers.
 *
 * The stream is made of packets, each containing N small frames of X samples. Frames are further divided into subframes.
 * XMA1/XMA2/WMAPRO only differ in the packet headers.
 */
static void ms_audio_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int bytes_per_packet, int samples_per_frame, int samples_per_subframe, int bits_frame_size) {
    int frames = 0, samples = 0, loop_start_frame = 0, loop_end_frame = 0, skip_packets;
#if XMA_CHECK_SKIPS
    int start_skip = 0, end_skip = 0, first_start_skip = 0, last_end_skip = 0;
#endif
    uint32_t first_frame_b, packet_skip_count = 0, frame_size_b, packet_size_b, header_size_b;
    uint64_t offset_b, packet_offset_b, frame_offset_b;
    size_t size;

    uint32_t packet_size = bytes_per_packet;
    off_t offset = msd->data_offset;
    uint32_t stream_offset_b = msd->data_offset * 8;

    size = offset + msd->data_size;
    packet_size_b = packet_size * 8;

    /* if we knew the streams mode then we could read just the first one and adjust samples later
     * not a big deal but maybe important for skip stuff */
    //streams = (msd->stream_mode==0 ? (msd->channels + 1) / 2 : msd->channels)
    skip_packets = 0;

    /* read packets */
    while (offset < size) {
        offset_b = offset * 8; /* global offset in bits */
        offset += packet_size; /* global offset in bytes */

        /* skip packets not owned by the first stream, since we only need samples from it */
        if (skip_packets && packet_skip_count) {
            packet_skip_count--;
            continue;
        }

        /* packet header */
        if (msd->xma_version == 1) { /* XMA1 */
            //packet_sequence = read_bitsBE_b(offset_b+0,  4,  streamFile); /* numbered from 0 to N */
            //unknown         = read_bitsBE_b(offset_b+4,  2,  streamFile); /* packet_metadata? (always 2) */
            first_frame_b     = read_bitsBE_b(offset_b+6,  bits_frame_size, streamFile); /* offset in bits inside the packet */
            packet_skip_count = read_bitsBE_b(offset_b+21, 11, streamFile); /* packets to skip for next packet of this stream */
            header_size_b     = 32;
        } else if (msd->xma_version == 2) { /* XMA2 */
            //frame_count     = read_bitsBE_b(offset_b+0,  6,  streamFile); /* frames that begin in this packet */
            first_frame_b     = read_bitsBE_b(offset_b+6,  bits_frame_size, streamFile); /* offset in bits inside this packet */
            //packet_metadata = read_bitsBE_b(offset_b+21, 3,  streamFile); /* packet_metadata (always 1) */
            packet_skip_count = read_bitsBE_b(offset_b+24, 8,  streamFile); /* packets to skip for next packet of this stream */
            header_size_b     = 32;
        } else { /* WMAPRO(v3) */
            //packet_sequence = read_bitsBE_b(offset_b+0,  4,  streamFile); /* numbered from 0 to N */
            //unknown         = read_bitsBE_b(offset_b+4,  2,  streamFile); /* packet_metadata? (always 2) */
            first_frame_b     = read_bitsBE_b(offset_b+6, bits_frame_size, streamFile);  /* offset in bits inside the packet */
            packet_skip_count = 0; /* xwma probably has no need to skip packets since it uses real multichannel ch audio */
            header_size_b     = 4+2+bits_frame_size; /* variable-size header */
        }


        /* full packet skip */
        if (packet_skip_count == 0x7FF) {
            packet_skip_count = 0;
            continue;
        }
        if (packet_skip_count > 255) { /* seen in some (converted?) XMA1 */
            packet_skip_count = 0;
        }
        VGM_ASSERT(packet_skip_count > 10, "XMA: found big packet skip %i\n", packet_skip_count);//a bit unusual...
        //VGM_LOG("packet: off=%x, ff=%i, ps=%i\n", offset, first_frame_b, packet_skip_b);

        packet_offset_b = header_size_b + first_frame_b; /* packet offset in bits */

        /* read packet frames */
        while (packet_offset_b < packet_size_b) {
            frame_offset_b = offset_b + packet_offset_b; /* in bits for aligment stuff */

            //todo not sure if frames or frames+1 (considering skip_samples)
            if (msd->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == msd->loop_start_b)
                loop_start_frame = frames;
            if (msd->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == msd->loop_end_b)
                loop_end_frame = frames;


            /* frame header */
            frame_size_b = read_bitsBE_b(frame_offset_b, bits_frame_size, streamFile);
            frame_offset_b += bits_frame_size;
            if (frame_size_b == 0) /* observed in some files with empty frames/packets */
                break;
            packet_offset_b += frame_size_b; /* including header */

#if 0
            {
                uint32_t frame_config
                frame_config = read_bitsBE_b(frame_offset_b, 15, streamFile);

                //VGM_LOG(" frame %04i: off_b=%I64x (~0x%I64x), fs_b=%i (~0x%x), fs=%x\n",frames, frame_offset_b, frame_offset_b/8, frame_size_b,frame_size_b/8, frame_config);

                //if (frame_config != 0x7f00) /* "contains all subframes"? */
                //    continue; // todo read packet end bit instead
            }
#endif
            frame_offset_b += 15; //todo bits_frame_size?

            if (frame_size_b == 0x7FFF) { /* end packet frame marker */
                break;
            }

#if XMA_CHECK_SKIPS
            // more header stuff (info from FFmpeg)
            {
                int flag;

                /* ignore "postproc transform" */
                if (msd->channels > 1) {
                    flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                    frame_offset_b += 1;
                    if (flag) {
                        flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                        frame_offset_b += 1;
                        if (flag) {
                            frame_offset_b += 1 + 4 * msd->channels*msd->channels; /* 4-something per double channel? */
                        }
                    }
                }

                /* get start/end skips to get the proper number of samples */ //todo check if first bit =1 means full 512 skip
                flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                frame_offset_b += 1;
                if (flag) {
                    int new_skip;

                    /* get start skip */
                    flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                    frame_offset_b += 1;
                    if (flag) {
                        VGM_LOG("start_skip at 0x%I64x\n", frame_offset_b);
                        new_skip = read_bitsBE_b(frame_offset_b, 10, streamFile);
                        frame_offset_b += 10;
                        VGM_ASSERT(start_skip, "XMA: more than one start_skip (%i)\n", new_skip);

                        if (new_skip > samples_per_frame) { /* from xmaencode */
                            VGM_LOG("XMA: bad start_skip (%i)\n", new_skip);
                            new_skip = samples_per_frame;
                        }

                        if (frames==0) first_start_skip = new_skip; /* sometimes in the middle */
                        start_skip += new_skip;
                    }

                    /* get end skip */
                    flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                    frame_offset_b += 1;
                    if (flag) {
                        VGM_LOG("end_skip at 0x%I64x\n", frame_offset_b);
                        new_skip = read_bitsBE_b(frame_offset_b, 10, streamFile);
                        frame_offset_b += 10;
                        VGM_ASSERT(end_skip, "XMA: more than one end_skip (%i)\n", new_skip);

                        if (new_skip > samples_per_frame) { /* from xmaencode  */
                            VGM_LOG("XMA: bad end_skip (%i)\n", new_skip);
                            new_skip = samples_per_frame;
                        }

                        last_end_skip = new_skip; /* not seen */
                        end_skip += new_skip;
                    }

                    VGM_LOG("  skip: st=%i, ed=%i\n", start_skip, end_skip);
                }
            }
#endif

            samples += samples_per_frame;
            frames++;
        }
    }

#if XMA_CHECK_SKIPS
    //todo this seems to usually work, but not always
    /* apply skips (not sure why 64, empty samples generated by the decoder not in the file?) */
    samples = samples + 64 - start_skip;
    samples = samples + 64 - end_skip;

    msd->skip_samples = 64 + samples_per_frame; //todo not always correct
#endif

    msd->num_samples = samples;

    if (msd->loop_flag && loop_end_frame > loop_start_frame) {
        msd->loop_start_sample = loop_start_frame * samples_per_frame + msd->loop_start_subframe * samples_per_subframe;
        msd->loop_end_sample = loop_end_frame * samples_per_frame + msd->loop_end_subframe * samples_per_subframe;
#if XMA_CHECK_SKIPS
        /* maybe this is needed */
        //msd->loop_start_sample -= msd->skip_samples;
        //msd->loop_end_sample -= msd->skip_samples;
#endif
    }
}

static int wma_get_samples_per_frame(int version, int sample_rate, uint32_t decode_flags) {
    int frame_len_bits;

    if (sample_rate <= 16000)
        frame_len_bits = 9;
    else if (sample_rate <= 22050 || (sample_rate <= 32000 && version == 1))
        frame_len_bits = 10;
    else if (sample_rate <= 48000 || version < 3)
        frame_len_bits = 11;
    else if (sample_rate <= 96000)
        frame_len_bits = 12;
    else
        frame_len_bits = 13;

    if (version == 3) {
        int tmp = decode_flags & 0x6;
        if (tmp == 0x2)
            ++frame_len_bits;
        else if (tmp == 0x4)
            --frame_len_bits;
        else if (tmp == 0x6)
            frame_len_bits -= 2;
    }

    return 1 << frame_len_bits;
}

void xma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile) {
    const int bytes_per_packet = 2048;
    const int samples_per_frame = 512;
    const int samples_per_subframe = 128;

    ms_audio_get_samples(msd, streamFile, bytes_per_packet, samples_per_frame, samples_per_subframe, 15);
}

void wmapro_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags) {
    const int version = 3; /* WMAPRO = WMAv3 */
    int bytes_per_packet = block_align;
    int samples_per_frame = 0;
    int samples_per_subframe = 0;
    int bits_frame_size = 0;

    /* do some WMAPRO setup (code from ffmpeg) */
    samples_per_frame = wma_get_samples_per_frame(version, sample_rate, decode_flags);

    /* max bits needed to represent this block_align */
    bits_frame_size = floor(log(block_align) / log(2)) + 4;

    /* not really needed as I've never seen loop subframe data for WMA (probably possible though)
     * (FFmpeg has code to get min_samples_per subframe) */
    samples_per_subframe = 0;

    /* signal it's not XMA */
    msd->xma_version = 0;

    ms_audio_get_samples(msd, streamFile, bytes_per_packet, samples_per_frame, samples_per_subframe, bits_frame_size);
}

void wma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags) {
    const int version = 2; /* WMAv1 rarely used */
    int use_bit_reservoir = 0; /* last packet frame can spill into the next packet */
    int samples_per_frame = 0;
    int num_frames = 0;

    samples_per_frame = wma_get_samples_per_frame(version, sample_rate, decode_flags);

    /* assumed (ASF has a flag for this but XWMA doesn't) */
    if (version == 2)
        use_bit_reservoir = 1;


    if (!use_bit_reservoir) {
        /* 1 frame per packet */
        num_frames = msd->data_size / block_align + (msd->data_size % block_align ? 1 : 0);
    }
    else {
        /* variable frames per packet (mini-header values) */
        off_t offset = msd->data_offset;
        while (offset < msd->data_size) { /* read packets (superframes) */
            int packet_frames;
            uint8_t header = read_8bit(offset, streamFile); /* upper nibble: index;  lower nibble: frames */

            /* frames starting in this packet (ie. counts frames that spill due to bit_reservoir) */
            packet_frames = (header & 0xf);

            num_frames += packet_frames;
            offset += block_align;
        }
    }

    msd->num_samples = num_frames * samples_per_frame;
}



/* ******************************************** */
/* HEADER PARSING                               */
/* ******************************************** */

/* Read values from a XMA1 RIFF "fmt" chunk (XMAWAVEFORMAT), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. */
void xma1_parse_fmt_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * loop_start_b, int32_t * loop_end_b, int32_t * loop_subframe, int be) {
    int16_t (*read_16bit)(off_t,STREAMFILE*) = be ? read_16bitBE : read_16bitLE;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = be ? read_32bitBE : read_32bitLE;
    int i, num_streams, total_channels = 0;

    if (read_16bit(chunk_offset+0x00,streamFile) != 0x165) return;

    num_streams = read_16bit(chunk_offset+0x08,streamFile);
    if(loop_flag)  *loop_flag = (uint8_t)read_8bit(chunk_offset+0xA,streamFile) > 0;

    /* sample rate and loop bit offsets are defined per stream, but the first is enough */
    if(sample_rate)   *sample_rate   = read_32bit(chunk_offset+0x10,streamFile);
    if(loop_start_b)  *loop_start_b  = read_32bit(chunk_offset+0x14,streamFile);
    if(loop_end_b)    *loop_end_b    = read_32bit(chunk_offset+0x18,streamFile);
    if(loop_subframe) *loop_subframe = (uint8_t)read_8bit(chunk_offset+0x1C,streamFile);

    /* channels is the sum of all streams */
    for (i = 0; i < num_streams; i++) {
        total_channels += read_8bit(chunk_offset+0x0C+0x11+i*0x10,streamFile);
    }
    if(channels)      *channels = total_channels;
}

/* Read values from a 'new' XMA2 RIFF "fmt" chunk (XMA2WAVEFORMATEX), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. Only the extra data, the first part is a normal WAVEFORMATEX. */
void xma2_parse_fmt_chunk_extra(STREAMFILE *streamFile, off_t chunk_offset, int * loop_flag, int32_t * num_samples, int32_t * loop_start_sample, int32_t * loop_end_sample, int be) {
    int16_t (*read_16bit)(off_t,STREAMFILE*) = be ? read_16bitBE : read_16bitLE;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = be ? read_32bitBE : read_32bitLE;

    if (read_16bit(chunk_offset+0x00,streamFile) != 0x166) return;
    /* up to extra data is a WAVEFORMATEX */
    if (read_16bit(chunk_offset+0x10,streamFile) < 0x22) return; /* expected extra data size */

    if(num_samples)        *num_samples       = read_32bit(chunk_offset+0x18,streamFile);
    if(loop_start_sample)  *loop_start_sample = read_32bit(chunk_offset+0x28,streamFile);
    if(loop_end_sample)    *loop_end_sample   = read_32bit(chunk_offset+0x28,streamFile) + read_32bit(chunk_offset+0x2C,streamFile);
    if(loop_flag)          *loop_flag = (uint8_t)read_8bit(chunk_offset+0x30,streamFile) > 0 /* never set in practice */
            || read_32bit(chunk_offset+0x2C,streamFile); /*loop_end_sample*/
    /* play_begin+end = probably pcm_samples (for original sample rate), don't seem to affect anything */
    /* int32_t play_begin_sample = read_32bit(xma->chunk_offset+0x20,streamFile); */
    /* int32_t play_end_sample = play_begin_sample + read_32bit(xma->chunk_offset+0x24,streamFile); */
}

/* Read values from an 'old' XMA2 RIFF "XMA2" chunk (XMA2WAVEFORMAT), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. */
void xma2_parse_xma2_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * num_samples, int32_t * loop_start_sample, int32_t * loop_end_sample) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = read_32bitBE; /* XMA2WAVEFORMAT is always big endian */
    int i, xma2_chunk_version, num_streams, total_channels = 0;
    off_t off;

    xma2_chunk_version = read_8bit(chunk_offset+0x00,streamFile);
    num_streams        = read_8bit(chunk_offset+0x01,streamFile);
    if(loop_start_sample)  *loop_start_sample = read_32bit(chunk_offset+0x04,streamFile);
    if(loop_end_sample)    *loop_end_sample   = read_32bit(chunk_offset+0x08,streamFile);
    if(loop_flag)          *loop_flag = (uint8_t)read_8bit(chunk_offset+0x03,streamFile) > 0 /* rarely not set, encoder default */
            || read_32bit(chunk_offset+0x08,streamFile); /* loop_end_sample */
    if(sample_rate) *sample_rate = read_32bit(chunk_offset+0x0c,streamFile);;

    off = xma2_chunk_version == 3 ? 0x14 : 0x1C;
    if(num_samples) *num_samples = read_32bit(chunk_offset+off,streamFile);
     /*xma->pcm_samples = read_32bitBE(xma->chunk_offset+off+0x04,streamFile)*/
    /* num_samples is the max samples in the file (apparently not including encoder delay) */
    /* pcm_samples are original WAV's; not current since samples and sample rate may be adjusted for looping purposes */

    off = xma2_chunk_version == 3 ? 0x20 : 0x28;
    /* channels is the sum of all streams */
    for (i = 0; i < num_streams; i++) {
        total_channels += read_8bit(chunk_offset+off+i*0x04,streamFile);
    }
    if (channels) *channels = total_channels;
}


/* ******************************************** */
/* OTHER STUFF                                  */
/* ******************************************** */

size_t atrac3_bytes_to_samples(size_t bytes, int full_block_align) {
    /* ATRAC3 expects full block align since as is can mix joint stereo with mono blocks;
     * so (full_block_align / channels) DOESN'T give the size of a single channel (uncommon in ATRAC3 though) */
    return (bytes / full_block_align) * 1024;
}

size_t atrac3plus_bytes_to_samples(size_t bytes, int full_block_align) {
    /* ATRAC3plus expects full block align since as is can mix joint stereo with mono blocks;
     * so (full_block_align / channels) DOESN'T give the size of a single channel (common in ATRAC3plus) */
    return (bytes / full_block_align) * 2048;
}
