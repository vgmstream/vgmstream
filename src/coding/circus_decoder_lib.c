#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Decodes Circus's audio codec, reverse engineered from various .exe.
 *
 * Some sources identify this codec as VQ (vector quantization), though vector(?)
 * data isn't actually bitpacked and just compressed using custom LZ or standard zlib.
 * Channels aren't divided either so decoding results in N-ch interleaved PCM.
 * It does seem to be using LPC/speech stuff from VQ codecs though.
 *
 * Some info from Japanese libpcm.c found in foo_adpcm
 * https://bitbucket.org/losnoco/foo_adpcm/src/master/foo_oki/source/libpcm/libpcm.cpp
 */

#include "circus_decoder_lib.h"
#include "circus_decoder_lib_data.h"

#include "circus_decoder_lzxpcm.h"

/* use miniz (API-compatible) to avoid adding external zlib just for this codec
 * - https://github.com/richgel999/miniz */
#include "../util/miniz.h"
//#include "zlib.h"


//#define XPCM_CODEC_PCM       0
#define XPCM_CODEC_VQ_LZXPCM   1
//#define XPCM_CODEC_ADPCM     2
#define XPCM_CODEC_VQ_DEFLATE  3

/* frame encodes 4096 PCM samples (all channels) = 4096*2 = 0x2000 bytes, re-interleaved then compressed */
#define XPCM_FRAME_SIZE         (4096 * 2)
#define XPCM_FRAME_CODES        4096
#define XPCM_FRAME_SAMPLES_ALL  4064
#define XPCM_FRAME_OVERLAP_ALL  32
#define XPCM_INPUT_SIZE         0x8000

/* ************************************************************************* */
/* DECODE */
/* ************************************************************************* */

struct circus_handle_t {
    /* config */
    off_t start;
    uint8_t codec;
    uint8_t flags;
    const int* scales;

    /* temp buffers */
    uint8_t srcbuf[XPCM_INPUT_SIZE];    /* compressed input data (arbitrary size) */
    uint8_t decbuf[XPCM_FRAME_SIZE];    /* single decompressed frame */
    uint8_t intbuf[XPCM_FRAME_SIZE];    /* re-interleaved frame */
    int32_t invbuf[XPCM_FRAME_CODES];   /* main LPC data (may need less) */
    int32_t tmpbuf[XPCM_FRAME_CODES];   /* temp LPC data (may need less) */

    /* output samples (original code reuses decbuf though) */
    int16_t pcmbuf[XPCM_FRAME_SAMPLES_ALL + XPCM_FRAME_OVERLAP_ALL]; /* final output samples and extra overlap samples */

    /* sample filter state */
    int hist1;
    int hist2;
    int frame;

    /* lz/deflate decompression state */
    lzxpcm_stream_t lstrm;
    z_stream dstrm;
    off_t offset;
};


static void convert(uint8_t flags, int32_t* invbuf, int16_t* pcmbuf, int* p_hist1, int* p_hist2, int frame) {
    int i;
    int sample, hist1, hist2;

    hist1 = *p_hist1;
    hist2 = *p_hist2;

    /* some ops below would use SHRs (>>), but there is some rounding in the original
     * ASM that decompiles and I think corresponds do DIVs (right shift and divs of
     * negative values isn't equivalent). Similarly the filters seem to use CDQ tricks
     * to simulate s64 ops, but I'm not sure casting is 100% equivalent (sounds ok tho). */

    /* do final filtering and conversion to PCM */
    for (i = 0; i < XPCM_FRAME_SAMPLES_ALL + XPCM_FRAME_OVERLAP_ALL; i++) {
        sample = *invbuf++;
        if (flags & 0x10)
            sample = (3 * (int64_t)sample / 2) / 1024; //>> 10;
        else
            sample = sample / 1024; //>> 10;

        sample = ((27 * (int64_t)sample + 4 * hist1 + hist2) << 11) / 65536; //>> 16

        hist2 = hist1;
        hist1 = sample;

        /* last 32 decoded samples aren't output, but are used next frame to overlap
         * with beginning samples (filters(?) windowing, not too noticeable though) */
        if (i < XPCM_FRAME_OVERLAP_ALL && frame > 0) {
            sample = ((i * (int64_t)sample) + ((XPCM_FRAME_OVERLAP_ALL - i) * pcmbuf[XPCM_FRAME_SAMPLES_ALL + i])) / 32; //>> 5
        }

        if (sample > 32767)
            sample = 32767;
        else if (sample < -32768)
            sample = -32768;

        pcmbuf[i] = sample;
    }

    *p_hist1 = hist1;
    *p_hist2 = hist2;
}

static void transform(int32_t* invbuf, int32_t* tmpbuf) {
    int lpc1, lpc2, lpc3, lpc4;
    int step1, step2, step3;
    int sc1, sc2;

    /* bits were originally configurable (passed arg), but actually called with const 12, 
     * and removed in later games along with superfluous ifs (coefs > 0, bits >= 3, etc) */
    //const int frame_bits = 12;

    step1 = 4096; /* 1 << 12 */
    step2 = step1 >> 1;
    step3 = step2 >> 1;
    sc1 = 1;

    /* inverse transform of LPC(?) coefs */
    for (lpc1 = 0; lpc1 < 12 - 2; lpc1++) {
        int sub1, sub2;
        int i1, i2, i3, i4;
        int64_t cos1, sin1, cos2, sin2; /* needs i64 to force 64b ops (avoid overflows) then converted to i32 */

        cos1 = (int64_t)sincos_table[sc1 + 1024];
        sin1 = (int64_t)sincos_table[sc1 + 0];

        i1 = 0;
        i2 = step2;
        i3 = step3;
        i4 = step2 + step3;

        for (lpc2 = 0; lpc2 < 4096; lpc2 += step1) {
            sub1 = invbuf[i1 + 0] - invbuf[i2 + 0];
            sub2 = tmpbuf[i1 + 0] - tmpbuf[i2 + 0];
            invbuf[i1 + 0] += invbuf[i2 + 0];
            tmpbuf[i1 + 0] += tmpbuf[i2 + 0];
            invbuf[i2 + 0] = sub1;
            tmpbuf[i2 + 0] = sub2;

            sub1 = invbuf[i1 + 1] - invbuf[i2 + 1];
            sub2 = tmpbuf[i1 + 1] - tmpbuf[i2 + 1];
            invbuf[i1 + 1] += invbuf[i2 + 1];
            tmpbuf[i1 + 1] += tmpbuf[i2 + 1];
            invbuf[i2 + 1] = (int32_t)( ((sub1 * cos1) >> 12) + ((sub2 * sin1) >> 12) );
            tmpbuf[i2 + 1] = (int32_t)( ((sub2 * cos1) >> 12) - ((sub1 * sin1) >> 12) );

            sub1 = invbuf[i3 + 0] - invbuf[i4 + 0];
            sub2 = tmpbuf[i3 + 0] - tmpbuf[i4 + 0];
            invbuf[i3 + 0] += invbuf[i4 + 0];
            tmpbuf[i3 + 0] += tmpbuf[i4 + 0];
            invbuf[i4 + 0] = sub2;
            tmpbuf[i4 + 0] = -sub1;

            sub1 = invbuf[i3 + 1] - invbuf[i4 + 1];
            sub2 = tmpbuf[i3 + 1] - tmpbuf[i4 + 1];
            invbuf[i3 + 1] += invbuf[i4 + 1];
            tmpbuf[i3 + 1] += tmpbuf[i4 + 1];
            invbuf[i4 + 1] = (int32_t)(   ((sub2 * cos1) >> 12) - ((sub1 * sin1) >> 12) );
            tmpbuf[i4 + 1] = (int32_t)( -(((sub1 * cos1) >> 12) + ((sub2 * sin1) >> 12)) );

            i1 += step1;
            i2 += step1;
            i3 += step1;
            i4 += step1;
        }

        if (step3 > 2) {
            sc2 = sc1 * 2;

            for (lpc3 = 2; lpc3 < step3; lpc3++) {
                cos2 = (int64_t)sincos_table[sc2 + 1024];
                sin2 = (int64_t)sincos_table[sc2 + 0];
                sc2 += sc1;

                i1 = 0 + lpc3;
                i2 = step2 + lpc3;
                i3 = step3 + lpc3;
                i4 = step2 + step3 + lpc3;

                for (lpc4 = 0; lpc4 < 4096; lpc4 += step1) {
                    sub1 = invbuf[i1] - invbuf[i2];
                    sub2 = tmpbuf[i1] - tmpbuf[i2];
                    invbuf[i1] += invbuf[i2];
                    tmpbuf[i1] += tmpbuf[i2];
                    invbuf[i2] = (int32_t)( ((sub1 * cos2) >> 12) + ((sub2 * sin2) >> 12) );
                    tmpbuf[i2] = (int32_t)( ((sub2 * cos2) >> 12) - ((sub1 * sin2) >> 12) );

                    sub1 = invbuf[i3] - invbuf[i4];
                    sub2 = tmpbuf[i3] - tmpbuf[i4];
                    invbuf[i3] += invbuf[i4];
                    tmpbuf[i3] += tmpbuf[i4];
                    invbuf[i4] = (int32_t)(  ((sub2 * cos2) >> 12) - ((sub1 * sin2) >> 12) );
                    tmpbuf[i4] = (int32_t)( -(((sub1 * cos2) >> 12) + ((sub2 * sin2) >> 12)) );

                    i1 += step1;
                    i2 += step1;
                    i3 += step1;
                    i4 += step1;
                }
            }
        }

        step1 = step2; // step1 >>= 1;
        step2 = step3; // step2 >>= 1;
        step3 >>= 1;
        sc1 *= 2;
    }

    {
        int i, j;
        int sub1, sub2, pow;

        for (i = 0; i < 4096; i += 4) {
            sub1 = invbuf[i + 0] - invbuf[i + 2];
            invbuf[i + 0] += invbuf[i + 2];
            invbuf[i + 2] = sub1;

            sub2 = tmpbuf[i + 0] - tmpbuf[i + 2];
            tmpbuf[i + 0] += tmpbuf[i + 2];
            tmpbuf[i + 2] = sub2;

            sub1 = invbuf[i + 3] - invbuf[i + 1];
            sub2 = tmpbuf[i + 1] - tmpbuf[i + 3];
            invbuf[i + 1] += invbuf[i + 3];
            invbuf[i + 3] = sub2;
            tmpbuf[i + 1] += tmpbuf[i + 3];
            tmpbuf[i + 3] = sub1;
        }

        for (i = 0; i < 4096; i += 2) {
            sub1 = invbuf[i + 0] - invbuf[i + 1];
            invbuf[i + 0] += invbuf[i + 1];
            invbuf[i + 1] = sub1;

            sub2 = tmpbuf[i + 0] - tmpbuf[i + 1];
            tmpbuf[i + 0] += tmpbuf[i + 1];
            tmpbuf[i + 1] = sub2;
        }

        for (i = 1, j = 0; i < 4096 - 1; i++) {
            for (pow = 4096 / 2; pow <= j; pow /= 2) {
                j -= pow;
            }
            j += pow;

            if (i < j) {
                sub1 = invbuf[j];
                invbuf[j] = invbuf[i];
                invbuf[i] = sub1;

                sub2 = tmpbuf[j];
                tmpbuf[j] = tmpbuf[i];
                tmpbuf[i] = sub2;
            }
        }
    }
}

static void scale(const uint8_t* intbuf, const int* scales, int32_t* invbuf, int32_t* tmpbuf) {
    int i, j;

    /* reinterleave and scale intbuf into invbuf and tmpbuf */
    for (i = 0, j = 0; i < 4096 / 2; i++, j += 16) {
        int scale, qv1, qv2;

        scale = scales[j / 4096];

        qv1 = (intbuf[i*4 + 0] << 0) | (intbuf[i*4 + 1] << 8); /* get_u16le */
        qv2 = (intbuf[i*4 + 2] << 0) | (intbuf[i*4 + 3] << 8); /* get_u16le */

        /* lowest bit is short of "positive" flag, or rather: even=0..-32767, odd=1..32768
         * (originally done through a LUT init at runtime with all 65536 indexes) */
        qv1 = (qv1 & 1) ? (qv1 >> 1) + 1 : -(qv1 >> 1);
        qv2 = (qv2 & 1) ? (qv2 >> 1) + 1 : -(qv2 >> 1);

        invbuf[i] = scale * qv1;
        tmpbuf[i] = scale * qv2;
    }

    /* reset rest of invbuf/tmpbuf */
    for (i = 4096 / 2; i < 4096; i++) {
        invbuf[i] = 0;
        tmpbuf[i] = 0;
    }
}

static void interleave(const uint8_t* decbuf, uint8_t* intbuf) {
    int i, j;

    /* reorder odd decbuf bytes into intbuf */
    for (i = 0, j = 1; i < 0x1000; i++, j += 2) {
        intbuf[j] = decbuf[i];
    }

    /* reorder even decbuf bytes into intbuf */
    for (i = 0x1000, j = 0; i < 0x1800; i++, j += 4) {
        uint8_t lo = decbuf[i + 0x800];
        uint8_t hi = decbuf[i];

        intbuf[j + 0] = (hi & 0xF0) | (lo >> 4);
        intbuf[j + 2] = (hi << 4) | (lo & 0x0F);
    }
}

/* ************************************************************ */
/* API */
/* ************************************************************ */

circus_handle_t* circus_init(off_t start, uint8_t codec, uint8_t flags) {
    circus_handle_t* handle = NULL;
    int scale_index, err;

    handle = malloc(sizeof(circus_handle_t));
    if (!handle) goto fail;

    handle->start = start;
    handle->codec = codec; //(config >> 0) & 0xFF;
    handle->flags = flags; //(config >> 8) & 0xFF;

    scale_index = (handle->flags & 0xF);
    if (scale_index > 5) goto fail;
    handle->scales = scale_table[scale_index];

    if (handle->codec == XPCM_CODEC_VQ_DEFLATE) {
        memset(&handle->dstrm, 0, sizeof(z_stream));
        err = inflateInit(&handle->dstrm);
        if (err < 0) goto fail;
    }

    circus_reset(handle);

    return handle;
fail:
    circus_free(handle);
    return NULL;
}

void circus_free(circus_handle_t* handle) {
    if (!handle)
        return;

    if (handle->codec == XPCM_CODEC_VQ_DEFLATE) {
        inflateEnd(&handle->dstrm);
    }

    free(handle);
}

void circus_reset(circus_handle_t* handle) {
    if (!handle)
        return;
    handle->hist1 = 0;
    handle->hist2 = 0;
    handle->frame = 0;

    if (handle->codec == XPCM_CODEC_VQ_LZXPCM) {
        lzxpcm_reset(&handle->lstrm);
    } else if (handle->codec == XPCM_CODEC_VQ_DEFLATE) {
        inflateReset(&handle->dstrm);
    }
    handle->offset = handle->start;
}

static int decompress_frame_lzxpcm(circus_handle_t* handle, STREAMFILE* sf) {
    int res;

    handle->lstrm.next_out = handle->decbuf;
    handle->lstrm.avail_out = sizeof(handle->decbuf);
    handle->lstrm.total_out = 0;
    do {
        if (handle->lstrm.avail_in == 0) {
            handle->lstrm.next_in = handle->srcbuf;
            handle->lstrm.avail_in = read_streamfile(handle->srcbuf, handle->offset, sizeof(handle->srcbuf), sf);
            handle->offset += handle->lstrm.avail_in;

            /* EOF (game reserves some extra buf so memset'ing is probably equivalent) */
            if (handle->lstrm.avail_in == 0) {
                memset(handle->decbuf + handle->lstrm.total_out, 0, sizeof(handle->decbuf) - handle->dstrm.total_out);
                break;
            }
        }

        res = lzxpcm_decompress(&handle->lstrm);
        if (res != LZXPCM_OK)
            goto fail;
    }
    while(handle->lstrm.avail_out != 0);

    return 1;
fail:
    return 0;
}

static int decompress_frame_deflate(circus_handle_t* handle, STREAMFILE* sf) {
    int res;

    handle->dstrm.next_out = handle->decbuf;
    handle->dstrm.avail_out = sizeof(handle->decbuf);
    handle->dstrm.total_out = 0;
    do {
        if (handle->dstrm.avail_in == 0) {
            handle->dstrm.next_in = handle->srcbuf;
            handle->dstrm.avail_in = read_streamfile(handle->srcbuf, handle->offset, sizeof(handle->srcbuf), sf);
            handle->offset += handle->dstrm.avail_in;

            /* EOF (game reserves some extra buf so memset'ing is probably equivalent) */
            if (handle->dstrm.avail_in == 0) {
                memset(handle->decbuf + handle->dstrm.total_out, 0, sizeof(handle->decbuf) - handle->dstrm.total_out);
                break;
            }
        }

        res = inflate(&handle->dstrm, Z_NO_FLUSH);
        if (res != Z_OK && res != Z_STREAM_END)
            goto fail;
    }
    while(handle->dstrm.avail_out != 0);

    return 1;
fail:
    return 0;
}

#ifdef XPCM_ALT
/* original code uses zlib 1.2.1 to decompress the full stream into memory */
static int deflate_decompress_full(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size) {
    int err;
    z_stream strm = {0};
    strm.next_in  = src;
    strm.avail_in = src_size;
    strm.next_out = dst;
    strm.avail_out = dst_size;

    err = inflateInit(&strm);
    if (err < 0) {
        //printf("inflateInit error: %i\n", err);
        return 0;
    }

    err = inflate(&strm, Z_FINISH);
    if (err < 0) {
        //printf("inflate error: %i\n", err);
        //return 0;
    }

    err = inflateEnd(&strm);
    if (err < 0) {
        //printf("inflateEnd error: %i\n", err);
        return 0;
    }

    return 0;
}
#endif

int circus_decode_frame(circus_handle_t* handle, STREAMFILE* sf, int16_t** p_buf, int* p_buf_samples_all) {
    int ok;

    if (handle->codec == XPCM_CODEC_VQ_LZXPCM) {
        ok = decompress_frame_lzxpcm(handle, sf);
    } else if (handle->codec == XPCM_CODEC_VQ_DEFLATE) {
        ok = decompress_frame_deflate(handle, sf);
    } else {
        ok = 0;
    }
    if (!ok)
        goto fail;

    interleave(handle->decbuf, handle->intbuf);
    scale(handle->intbuf, handle->scales, handle->invbuf, handle->tmpbuf);
    transform(handle->invbuf, handle->tmpbuf);
    convert(handle->flags, handle->invbuf, handle->pcmbuf, &handle->hist1, &handle->hist2, handle->frame);
    handle->frame++;

    *p_buf = handle->pcmbuf;
    *p_buf_samples_all = XPCM_FRAME_SAMPLES_ALL;
    return 1;
fail:
    return 0;
}
