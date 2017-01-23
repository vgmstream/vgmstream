#include "meta.h"
#include "../util.h"

#ifdef VGM_USE_FFMPEG

#define ADJUST_SAMPLE_RATE              0
#define XMA_BYTES_PER_PACKET            2048
#define XMA_SAMPLES_PER_FRAME           512
#define XMA_SAMPLES_PER_SUBFRAME        128
#define FAKE_RIFF_BUFFER_SIZE           100


/* parsing helper */
typedef struct {
    size_t file_size;
    /* file traversing */
    int big_endian;
    off_t chunk_offset; /* main header chunk offset, after "(id)" and size */
    size_t chunk_size;
    off_t data_offset;
    size_t data_size;

    int32_t fmt_codec;
    uint8_t xma2_version;
    int needs_header;
    int force_little_endian;  /* FFmpeg can't parse big endian "fmt" chunks */

    /* info */
    int loop_flag;
    int32_t num_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;

    int32_t xma1_loop_start_offset_b;
    int32_t xma1_loop_end_offset_b;
    int32_t xma1_subframe_data;
} xma_header_data;


static int parse_header(xma_header_data * xma, STREAMFILE *streamFile);
static void parse_xma1_sample_data(xma_header_data * xma, STREAMFILE *streamFile);
static int create_riff_header(uint8_t * buf, size_t buf_size, xma_header_data * xma, STREAMFILE *streamFile);
static int fmt_chunk_swap_endian(uint8_t * chunk, uint16_t codec);
#if ADJUST_SAMPLE_RATE
static int get_xma_sample_rate(int32_t general_rate);
#endif

/**
 * XMA 1/2 (Microsoft)
 *
 * Usually in RIFF headers and minor variations.
 */
VGMSTREAM * init_vgmstream_xma(STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    VGMSTREAM * vgmstream = NULL;
    ffmpeg_codec_data *data = NULL;

    xma_header_data xma;
    uint8_t fake_riff[FAKE_RIFF_BUFFER_SIZE];
    int fake_riff_size = 0;


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xma",filename_extension(filename))
            && strcasecmp("xma2",filename_extension(filename))  /* Skullgirls */
            && strcasecmp("nps",filename_extension(filename))   /* Beautiful Katamari */
            && strcasecmp("past",filename_extension(filename))  /* SoulCalibur II HD */
            )
        goto fail;

    /* check header */
    if ( !parse_header(&xma, streamFile) )
        goto fail;


    /* init ffmpeg (create a fake RIFF that FFmpeg can read if needed) */
    if (xma.needs_header) { /* fake header + partial size */
        fake_riff_size = create_riff_header(fake_riff, FAKE_RIFF_BUFFER_SIZE, &xma, streamFile);
        if (fake_riff_size <= 0) goto fail;

        data = init_ffmpeg_header_offset(streamFile, fake_riff, (uint64_t)fake_riff_size, xma.data_offset, xma.data_size);
        if (!data) goto fail;
    }
    else { /* no change */
        data = init_ffmpeg_offset(streamFile, 0, xma.file_size);
        if (!data) goto fail;
    }


    /* build VGMSTREAM */
    vgmstream = allocate_vgmstream(data->channels, xma.loop_flag);
    if (!vgmstream) goto fail;
    /*vgmstream->channels = data->channels;*/
    /*vgmstream->loop_flag = loop_flag;*/

    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;

    vgmstream->sample_rate = data->sampleRate;
#if ADJUST_SAMPLE_RATE
    vgmstream->sample_rate = get_xma_sample_rate(vgmstream->sample_rate);
#endif
    vgmstream->num_samples = xma.num_samples; /* data->totalSamples: XMA1 = not set; XMA2 = not reliable  */

    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = xma.loop_start_sample;
        vgmstream->loop_end_sample = xma.loop_end_sample;
    }


    return vgmstream;

fail:
    /* clean up */
    if (data) {
        free_ffmpeg(data);
    }
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    return NULL;
}


/**
 * Finds stuff needed for XMA with FFmpeg
 *
 * returns 1 if ok, 0 on error
 */
static int parse_header(xma_header_data * xma, STREAMFILE *streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    uint32_t id;
    int big_endian = 0;
    enum {
        id_RIFF = UINT32_C(0x52494646),  /* "RIFF" */
        id_RIFX = UINT32_C(0x52494658),  /* "RIFX" */
        id_NXMA = UINT32_C(0x786D6100),  /* "xma\0" */
        id_PASX = UINT32_C(0x50415358),  /* "PASX" */
    };


    /* check header */
    id = read_32bitBE(0x00,streamFile);
    switch (id) {
        case id_RIFF:
            break;
        case id_RIFX:
        case id_NXMA:
        case id_PASX:
            big_endian = 1;
            break;
        default:
            goto fail;
    }

    memset(xma,0,sizeof(xma_header_data));
    xma->big_endian = big_endian;

    if (xma->big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    xma->file_size = streamFile->get_size(streamFile);

    /* find offsets */
    if (id == id_RIFF || id == id_RIFX) { /* regular RIFF header */
        off_t current_chunk = 0xc;
        off_t fmt_offset = 0, xma2_offset = 0;
        size_t riff_size = 0, fmt_size = 0, xma2_size = 0;

        riff_size = read_32bit(4,streamFile);
        if (riff_size+8 > xma->file_size) goto fail;

        while (current_chunk < xma->file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bit(current_chunk+4,streamFile);

            if (current_chunk+4+4+chunk_size > xma->file_size)
                goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    if (fmt_offset) goto fail;

                    fmt_offset = current_chunk + 4 + 4;
                    fmt_size = chunk_size;
                    break;
                case 0x64617461:    /* "data" */
                    if (xma->data_offset) goto fail;

                    xma->data_offset = current_chunk + 4 + 4;
                    xma->data_size = chunk_size;
                    break;
                case 0x584D4132:    /* "XMA2" */
                    if (xma2_offset) goto fail;

                    xma2_offset = current_chunk + 4 + 4;
                    xma2_size = chunk_size;
                    break;
                default:
                    break;
            }

            current_chunk += 8+chunk_size;
        }

        /* give priority to "XMA2" since it can go together with "fmt " */
        if (xma2_offset) {
            xma->chunk_offset = xma2_offset;
            xma->chunk_size = xma2_size;
            xma->xma2_version = read_8bit(xma->chunk_offset,streamFile);
            xma->needs_header = 1; /* FFmpeg can only parse pure XMA1 or pure XMA2 */
        } else if (fmt_offset) {
            xma->chunk_offset = fmt_offset;
            xma->chunk_size = fmt_size;
            xma->fmt_codec = read_16bit(xma->chunk_offset,streamFile);
            xma->force_little_endian = xma->big_endian;
        } else {
            goto fail;
        }
    }
    else if (id == id_NXMA) { /* Namco (Tekken 6, Galaga Legions DX) */
	    /* custom header with a "XMA2" or "fmt " data chunk inside, most other values are unknown */
        uint32_t chunk_type = read_32bit(0xC,streamFile);
        xma->data_offset = 0x100;
        xma->data_size = read_32bit(0x14,streamFile);
        xma->chunk_offset = 0xBC;
        xma->chunk_size = read_32bit(0x24,streamFile);
        if (chunk_type == 0x4) { /* "XMA2" */
            xma->xma2_version = read_8bit(xma->chunk_offset,streamFile);
        } else if (chunk_type == 0x8) { /* "fmt " */
            xma->fmt_codec = read_16bit(xma->chunk_offset,streamFile);
            xma->force_little_endian = 1;
        } else {
            goto fail;
        }
        xma->needs_header = 1;

        if (xma->data_size + xma->data_offset > xma->file_size) goto fail;
    }
    else if (id == id_PASX) {  /* SoulCalibur II HD */
	    /* custom header with a "fmt " data chunk inside */
        xma->chunk_size = read_32bit(0x08,streamFile);
        xma->data_size = read_32bit(0x0c,streamFile);
        xma->chunk_offset = read_32bit(0x10,streamFile);
        /* 0x14: chunk offset end */
        xma->data_offset = read_32bit(0x18,streamFile);
        xma->fmt_codec = read_16bit(xma->chunk_offset,streamFile);
        xma->needs_header = 1;
        xma->force_little_endian = 1;
    }
    else {
        goto fail;
    }


    /* find sample data */
    if (xma->xma2_version) { /* old XMA2 (internally always BE) */
        xma->loop_start_sample = read_32bitBE(xma->chunk_offset+0x4,streamFile);
        xma->loop_end_sample = read_32bitBE(xma->chunk_offset+0x8,streamFile);
        xma->loop_flag = (uint8_t)read_8bit(xma->chunk_offset+0x3,streamFile) > 0 /* rarely not set */
                || xma->loop_end_sample;
        if (xma->xma2_version == 3) {
            xma->num_samples = read_32bitBE(xma->chunk_offset+0x14,streamFile);
        } else {
            xma->num_samples = read_32bitBE(xma->chunk_offset+0x1C,streamFile);
        }
    }
    else if (xma->fmt_codec == 0x166) { /* pure XMA2 */
        xma->num_samples = read_32bit(xma->chunk_offset+0x18,streamFile);
        xma->loop_start_sample = read_32bit(xma->chunk_offset+0x28,streamFile);
        xma->loop_end_sample = xma->loop_start_sample + read_32bit(xma->chunk_offset+0x2C,streamFile);
        xma->loop_flag = (uint8_t)read_8bit(xma->chunk_offset+0x30,streamFile) > 0 /* never set in practice */
                || xma->loop_end_sample;
        /* not needed but may affect looping? (sometimes these don't match loop/total samples) */
        /* int32_t play_begin_sample = read_32bit(xma->chunk_offset+0x20,streamFile); */
        /* int32_t play_end_sample = play_begin_sample + read_32bit(xma->chunk_offset+0x24,streamFile); */
    }
    else if (xma->fmt_codec == 0x165) { /* pure XMA1 */
        xma->loop_flag = (uint8_t)read_8bit(xma->chunk_offset+0xA,streamFile) > 0;
        xma->xma1_loop_start_offset_b = read_32bit(xma->chunk_offset+0x14,streamFile);
        xma->xma1_loop_end_offset_b = read_32bit(xma->chunk_offset+0x18,streamFile);
        xma->xma1_subframe_data = (uint8_t)read_8bit(xma->chunk_offset+0x1C,streamFile);
        /* find samples count + loop samples since they are not in the header */
        parse_xma1_sample_data(xma, streamFile);
    }
    else { /* unknown chunk */
        goto fail;
    }

    return 1;

fail:
    return 0;
}


/**
 * XMA1: manually find total and loop samples
 *
 * A XMA1 stream is made of packets, each containing N frames of X samples, and frame is divided into subframes for looping purposes.
 * FFmpeg can't get total samples without decoding, so we'll count frames+samples by reading packet headers.
 */
static void parse_xma1_sample_data(xma_header_data * xma, STREAMFILE *streamFile) {
    int frames = 0, loop_frame_start = 0, loop_frame_end = 0, loop_subframe_end, loop_subframe_skip;
    uint32_t header, first_frame_b, packet_skip_b, frame_size_b, packet_size_b;
    uint64_t packet_offset_b, frame_offset_b;
    uint32_t size;

    uint32_t packet_size = XMA_BYTES_PER_PACKET;
    uint32_t offset = xma->data_offset;
    uint32_t offset_b = 0;
    uint32_t stream_offset_b = xma->data_offset * 8;

    size = offset + xma->data_size;
    packet_size_b = packet_size*8;

    while (offset < size) { /* stream global offset*/
        /* XMA1 packet header (32b) = packet_sequence:4, unk:2: frame_offset_in_bits:15, packet_stream_skip_count:11 */
        header = read_32bitBE(offset, streamFile);
        first_frame_b = (header >> 11) & 0x7FFF;
        packet_skip_b = (header) & 0x7FF;

        offset_b = offset * 8;
        packet_offset_b = 4*8 + first_frame_b;
        while (packet_offset_b < packet_size_b && packet_skip_b!=0x7FF) { /* internal packet offset + full packet skip (needed?) */
            frame_offset_b = offset_b + packet_offset_b; /* global offset to packet, in bits for aligment stuff */

            /* XMA1 frame header (32b) = frame_length_in_bits:15, frame_data:17+ */
            header = read_32bitBE(frame_offset_b/8, streamFile);
            frame_size_b = (header >> (17 - frame_offset_b % 8)) & 0x7FFF;

            if (frame_size_b == 0) /* observed in some files */
                break;

            packet_offset_b += frame_size_b;/* including header */

            if (frame_size_b != 0x7FFF) /* end frame marker*/
                frames++;

            if (xma->loop_flag && frame_offset_b - stream_offset_b == xma->xma1_loop_start_offset_b)
                loop_frame_start = frames;
            if (xma->loop_flag && frame_offset_b - stream_offset_b == xma->xma1_loop_end_offset_b)
                loop_frame_end = frames;
        }

        offset += packet_size;
    }

    loop_subframe_end = xma->xma1_subframe_data >> 4; /* upper 4b: subframe where the loop ends, 0..3 */
    loop_subframe_skip = xma->xma1_subframe_data & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */

    xma->num_samples = frames * XMA_SAMPLES_PER_FRAME;
    if (xma->loop_flag) {
        xma->loop_start_sample = loop_frame_start * XMA_SAMPLES_PER_FRAME + loop_subframe_skip * XMA_SAMPLES_PER_SUBFRAME;
        xma->loop_end_sample = loop_frame_end * XMA_SAMPLES_PER_FRAME + loop_subframe_end * XMA_SAMPLES_PER_SUBFRAME;
    }
}


/**
 * Recreates a RIFF header that FFmpeg can read since it lacks support for some variations.
 *
 * returns bytes written (up until "data" chunk + size), -1 on failure
 */
static int create_riff_header(uint8_t * buf, size_t buf_size, xma_header_data * xma, STREAMFILE *streamFile) {
    void (*put_32bit)(uint8_t *, int32_t) = NULL;
    uint8_t chunk[FAKE_RIFF_BUFFER_SIZE];
    uint8_t internal[FAKE_RIFF_BUFFER_SIZE];
    size_t head_size, file_size, internal_size;

    int use_be = xma->big_endian && !xma->force_little_endian;

    if (use_be) {
        put_32bit = put_32bitBE;
    } else {
        put_32bit = put_32bitLE;
    }

    memset(buf,0, sizeof(uint8_t) * buf_size);
    if (read_streamfile(chunk,xma->chunk_offset,xma->chunk_size, streamFile) != xma->chunk_size)
        goto fail;

    /* create internal chunks */
    if (xma->xma2_version == 3) { /* old XMA2 v3: change to v4 (extra 8 bytes in the middle) */
        internal_size = 4+4+xma->chunk_size + 8;

        memcpy(internal + 0x0, "XMA2", 4);  /* "XMA2" chunk (internal data is BE) */
        put_32bit(internal + 0x4, xma->chunk_size + 8); /* v3 > v4 size*/
        put_8bit(internal + 0x8, 4); /* v4 */
        memcpy(internal + 0x9, chunk+1, 15); /* first v3 part (fixed) */
        put_32bitBE(internal + 0x18, 0); /* extra v4 BE: "EncodeOptions" (not used by FFmpeg) */
        put_32bitBE(internal + 0x1c, 0); /* extra v4 BE: "PsuedoBytesPerSec" (not used by FFmpeg) */
        memcpy(internal + 0x20, chunk+16, xma->chunk_size - 16); /* second v3 part (variable) */
    }
    else { /* direct copy (old XMA2 v4 ignoring "fmt", pure XMA1/2) */
        internal_size = 4+4+xma->chunk_size;

        if (xma->force_little_endian ) {
            if ( !fmt_chunk_swap_endian(chunk, xma->fmt_codec) )
                goto fail;
        }

        memcpy(internal + 0x0, xma->xma2_version ? "XMA2" : "fmt ", 4);
        put_32bit(internal + 0x4, xma->chunk_size);
        memcpy(internal + 0x8, chunk, xma->chunk_size);
    }

    /* create main RIFF */
    head_size = 4+4 + 4 + internal_size + 4+4;
    file_size = head_size-4-4 + xma->data_size;
    if (head_size > buf_size) goto fail;

    memcpy(buf + 0x0, use_be ? "RIFX" : "RIFF", 4);
    put_32bit(buf + 0x4, file_size);
    memcpy(buf + 0x8, "WAVE", 4);
    memcpy(buf + 0xc, internal, internal_size);
    memcpy(buf + head_size-4-4, "data", 4);
    put_32bit(buf + head_size-4, xma->data_size);

    return head_size;

fail:
    return -1;
}


/**
 * Swaps endianness
 *
 * returns 0 on error
 */
static int fmt_chunk_swap_endian(uint8_t * chunk, uint16_t codec) {
    if (codec != 0x166)
        goto fail;

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

    return 1;

fail:
    return 0;
}


#if ADJUST_SAMPLE_RATE
/**
 * Get real XMA sample rate (from Microsoft docs, apparently info only and not correct for playback).
 */
static int32_t get_xma_sample_rate(int32_t general_rate) {
    int32_t xma_rate = 48000; /* default XMA */

    if (general_rate <= 24000)      xma_rate = 24000;
    else if (general_rate <= 32000) xma_rate = 32000;
    else if (general_rate <= 44100) xma_rate = 44100;

    return xma_rate;
}
#endif

#endif
