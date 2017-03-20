#include "coding.h"
#include "../vgmstream.h"

#ifdef VGM_USE_FFMPEG

#define XMA_CHECK_SKIPS                 0
#define XMA_BYTES_PER_PACKET            2048
#define XMA_SAMPLES_PER_FRAME           512
#define XMA_SAMPLES_PER_SUBFRAME        128

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

int ffmpeg_make_riff_xma_from_fmt(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian) {
    size_t riff_size = 4+4+ 4 + 4+4+fmt_size + 4+4;
    uint8_t chunk[100];

    if (buf_size < riff_size || fmt_size > 100)
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

int ffmpeg_make_riff_xwma(uint8_t * buf, size_t buf_size, int codec, size_t sample_count, size_t data_size, int channels, int sample_rate, int avg_bps, int block_align) {
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
    put_32bitLE(buf+0x1c, avg_bps); /* average bits per second, somehow vital for XWMA */
    put_16bitLE(buf+0x20, block_align); /* block align */
    put_16bitLE(buf+0x22, 16); /* bits per sample */
    put_16bitLE(buf+0x24, 0); /* unk */
    /* here goes the "dpds" table, but it's not needed by FFmpeg */

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

/**
 * Find total and loop samples by reading XMA frame headers.
 *
 * A XMA stream is made of packets, each containing N small frames of X samples.
 * Frames are further divided into subframes for looping purposes.
 * XMA1 and XMA2 only differ in the packet headers.
 */
void xma_get_samples(xma_sample_data * xma, STREAMFILE *streamFile) {
    int frames = 0, samples = 0, loop_start_frame = 0, loop_end_frame = 0, skip_packets;
#if XMA_CHECK_SKIPS
    int start_skip = 0, end_skip = 0, first_start_skip = 0, last_end_skip = 0;
#endif
    uint32_t first_frame_b, packet_skip_count = 0, frame_size_b, packet_size_b;
    uint64_t offset_b, packet_offset_b, frame_offset_b;
    size_t size;

    uint32_t packet_size = XMA_BYTES_PER_PACKET;
    off_t offset = xma->data_offset;
    uint32_t stream_offset_b = xma->data_offset * 8;

    size = offset + xma->data_size;
    packet_size_b = packet_size * 8;

    /* if we knew the streams mode then we could read just the first one and adjust samples later
     * not a big deal but maybe important for skip stuff */
    //streams = (xma->stream_mode==0 ? (xma->channels + 1) / 2 : xma->channels)
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

        /* XMA1 or XMA2 packet header */
        if (xma->xma_version == 1) {
            //packet_sequence = read_bitsBE_b(offset_b+0,  4,  streamFile); /* numbered from 0 to N */
            //unknown         = read_bitsBE_b(offset_b+4,  2,  streamFile); /* packet_metadata? (always 2) */
            first_frame_b     = read_bitsBE_b(offset_b+6,  15, streamFile); /* offset in bits inside the packet */
            packet_skip_count = read_bitsBE_b(offset_b+21, 11, streamFile); /* packets to skip for next packet of this stream */
        } else {
            //frame_count     = read_bitsBE_b(offset_b+0,  6,  streamFile); /* frames that begin in this packet */
            first_frame_b     = read_bitsBE_b(offset_b+6,  15, streamFile); /* offset in bits inside this packet */
            //packet_metadata = read_bitsBE_b(offset_b+21, 3,  streamFile); /* packet_metadata (always 1) */
            packet_skip_count = read_bitsBE_b(offset_b+24, 8,  streamFile); /* packets to skip for next packet of this stream */
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

        packet_offset_b = 4*8 + first_frame_b; /* packet offset in bits */

        /* read packet frames */
        while (packet_offset_b < packet_size_b) {
            frame_offset_b = offset_b + packet_offset_b; /* in bits for aligment stuff */

            //todo not sure if frames or frames+1 (considering skip_samples)
            if (xma->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == xma->loop_start_b)
                loop_start_frame = frames;
            if (xma->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == xma->loop_end_b)
                loop_end_frame = frames;


            /* XMA1/2 frame header */
            frame_size_b = read_bitsBE_b(frame_offset_b, 15, streamFile);
            frame_offset_b += 15;
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
            frame_offset_b += 15;

            if (frame_size_b == 0x7FFF) { /* end packet frame marker */
                break;
            }

#if XMA_CHECK_SKIPS
            // more header stuff (info from FFmpeg)
            {
                int flag;

                /* ignore "postproc transform" */
                if (xma->channels > 1) {
                    flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                    frame_offset_b += 1;
                    if (flag) {
                        flag = read_bitsBE_b(frame_offset_b, 1, streamFile);
                        frame_offset_b += 1;
                        if (flag) {
                            frame_offset_b += 1 + 4 * xma->channels*xma->channels; /* 4-something per double channel? */
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

                        if (new_skip > XMA_SAMPLES_PER_FRAME) { /* from xmaencode */
                            VGM_LOG("XMA: bad start_skip (%i)\n", new_skip);
                            new_skip = XMA_SAMPLES_PER_FRAME;
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

                        if (new_skip > XMA_SAMPLES_PER_FRAME) { /* from xmaencode  */
                            VGM_LOG("XMA: bad end_skip (%i)\n", new_skip);
                            new_skip = XMA_SAMPLES_PER_FRAME;
                        }

                        last_end_skip = new_skip; /* not seen */
                        end_skip += new_skip;
                    }

                    VGM_LOG("  skip: st=%i, ed=%i\n", start_skip, end_skip);
                }
            }
#endif

            samples += XMA_SAMPLES_PER_FRAME;
            frames++;
        }
    }

#if XMA_CHECK_SKIPS
    //todo this seems to usually work, but not always
    /* apply skips (not sure why 64, empty samples generated by the decoder not in the file?) */
    samples = samples + 64 - start_skip;
    samples = samples + 64 - end_skip;

    xma->skip_samples = 64 + 512; //todo not always correct
#endif

    xma->num_samples = samples;

    if (xma->loop_flag && loop_end_frame > loop_start_frame) {
        xma->loop_start_sample = loop_start_frame * XMA_SAMPLES_PER_FRAME + xma->loop_start_subframe * XMA_SAMPLES_PER_SUBFRAME;
        xma->loop_end_sample = loop_end_frame * XMA_SAMPLES_PER_FRAME + xma->loop_end_subframe * XMA_SAMPLES_PER_SUBFRAME;
#if XMA_CHECK_SKIPS
        /* maybe this is needed */
        //xma->loop_start_sample -= xma->skip_samples;
        //xma->loop_end_sample -= xma->skip_samples;
#endif
    }
}

#endif
