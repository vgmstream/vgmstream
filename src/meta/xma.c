#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG

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
    int skip_samples;

    /* info */
    int channels;
    int loop_flag;
    int32_t num_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;

    int32_t loop_start_b;
    int32_t loop_end_b;
    int32_t loop_subframe;

    meta_t meta;
} xma_header_data;

static int parse_header(xma_header_data * xma, STREAMFILE *streamFile);
static void fix_samples(xma_header_data * xma, STREAMFILE *streamFile);
static int create_riff_header(uint8_t * buf, size_t buf_size, xma_header_data * xma, STREAMFILE *streamFile);

/**
 * XMA 1/2 (Microsoft)
 *
 * Usually in RIFF headers and minor variations.
 */
VGMSTREAM * init_vgmstream_xma(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    ffmpeg_codec_data *data = NULL;

    xma_header_data xma;
    uint8_t fake_riff[FAKE_RIFF_BUFFER_SIZE];
    int fake_riff_size = 0;


    /* check extension, case insensitive */
    /* .xma2: Skullgirls, .nps: Beautiful Katamari, .past: SoulCalibur II HD */
    if ( !check_extensions(streamFile, "xma,xma2,nps,past") )
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
    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = xma.meta;

    vgmstream->sample_rate = data->sampleRate;

    /* fix samples for some formats (data->totalSamples: XMA1 = not set; XMA2 = not reliable) */
    xma.channels = data->channels;
    fix_samples(&xma, streamFile);

    vgmstream->num_samples = xma.num_samples;
    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = xma.loop_start_sample;
        vgmstream->loop_end_sample = xma.loop_end_sample;
    }
#if 0
    //not active due to a FFmpeg bug that misses some of the last packet samples and decodes
    // garbage if asked for more samples (always happens but more apparent with skip_samples active)
    /* fix encoder delay */
    if (data->skipSamples==0)
        ffmpeg_set_skip_samples(data, xma.skip_samples);
#endif


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
        id_NXMA = UINT32_C(0x786D6100),  /* "xma\0" */
        id_PASX = UINT32_C(0x50415358),  /* "PASX" */
    };


    /* check header */
    id = read_32bitBE(0x00,streamFile);
    switch (id) {
        case id_RIFF:
            break;
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
    if (id == id_RIFF) { /* regular RIFF header */
        off_t current_chunk = 0xc;
        off_t fmt_offset = 0, xma2_offset = 0;
        size_t riff_size = 0, fmt_size = 0, xma2_size = 0;

        xma->meta = meta_XMA_RIFF;
        riff_size = read_32bit(4,streamFile);
        if (riff_size != xma->file_size &&  /* some Beautiful Katamari, unsure if bad rip */
            riff_size+8 > xma->file_size) goto fail;

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
    else if (id == id_NXMA) { /* Namco NUB xma (Tekken 6, Galaga Legions DX) */
        /* Custom header with a "XMA2" or "fmt " data chunk inside; most other values are unknown
         * It's here rather than its own meta to reuse the chunk parsing (probably intended to be .nub) */
        uint32_t chunk_type = read_32bit(0xC,streamFile);

        xma->meta = meta_NUB_XMA;
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
        /* Custom header with a "fmt " data chunk inside
         * It's here rather than its own meta to reuse the chunk parsing */

        xma->meta = meta_X360_PASX;
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

    /* parse sample data */
    if (xma->xma2_version) { /* old XMA2 */
        xma2_parse_xma2_chunk(streamFile, xma->chunk_offset, NULL,NULL, &xma->loop_flag, &xma->num_samples, &xma->loop_start_sample, &xma->loop_end_sample);
    }
    else if (xma->fmt_codec == 0x166) { /* pure XMA2 */
        xma2_parse_fmt_chunk_extra(streamFile, xma->chunk_offset, &xma->loop_flag, &xma->num_samples, &xma->loop_start_sample, &xma->loop_end_sample, xma->big_endian);
    }
    else if (xma->fmt_codec == 0x165) { /* pure XMA1 */
        xma1_parse_fmt_chunk(streamFile, xma->chunk_offset, NULL,NULL, &xma->loop_flag, &xma->loop_start_b, &xma->loop_end_b, &xma->loop_subframe, xma->big_endian);
    }
    else { /* unknown chunk */
        goto fail;
    }

    return 1;

fail:
    return 0;
}


static void fix_samples(xma_header_data * xma, STREAMFILE *streamFile) {
    ms_sample_data msd;

    /* for now only XMA1 is fixed, but xmaencode.exe doesn't seem to use
     * XMA2 sample values in the headers, and the exact number of samples may not be exact.
     * Also loop values don't seem to need skip_samples. */

    if (xma->fmt_codec != 0x165) {
        return;
    }

    memset(&msd,0,sizeof(ms_sample_data));

    /* call xma parser (copy to its own struct, a bit clunky but oh well...) */
    msd.xma_version = xma->fmt_codec==0x165 ? 1 : 2;
    msd.channels = xma->channels;
    msd.data_offset = xma->data_offset;
    msd.data_size = xma->data_size;
    msd.loop_flag = xma->loop_flag;
    msd.loop_start_b = xma->loop_start_b;
    msd.loop_end_b = xma->loop_end_b;
    msd.loop_start_subframe = xma->loop_subframe & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
    msd.loop_end_subframe = xma->loop_subframe >> 4; /* upper 4b: subframe where the loop ends, 0..3 */

    xma_get_samples(&msd, streamFile);

    /* and recieve results */
    xma->num_samples = msd.num_samples;
    xma->skip_samples = msd.skip_samples;
    xma->loop_start_sample = msd.loop_start_sample;
    xma->loop_end_sample = msd.loop_end_sample;
    /* XMA2 loop/num_samples don't seem to skip_samples */
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
            if ( !ffmpeg_fmt_chunk_swap_endian(chunk, xma->chunk_size, xma->fmt_codec) )
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

#endif
