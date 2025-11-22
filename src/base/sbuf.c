#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "sbuf.h"
#include "../util/log.h"

// float-to-int modes
//#define PCM16_ROUNDING_LRINT  // rounding down, potentially faster in some systems/compilers and much slower in others (also affects rounding)
//#define PCM16_ROUNDING_HALF   // rounding half + down (vorbis-style), more 'accurate' but slower

#if defined(PCM16_ROUNDING_HALF) || defined(PCM16_ROUNDING_LRINT)
#include <math.h>
#endif

/* when casting float to int, value is simply truncated:
 * - (int)1.7 = 1, (int)-1.7 = -1
 * alts for more accurate rounding could be:
 * - (int)floor(f)
 * - (int)(f < 0 ? f - 0.5f : f + 0.5f)
 * - (((int) (f1 + 32767.5)) - 32767)
 * - etc
 * but since +-1 isn't really audible we'll just cast, as it's the fastest
 *
 * Regular C float-to-int casting ("int i = (int)f") is somewhat slow due to IEEE
 * float requirements, but C99 adds some faster-but-less-precise casting functions.
 * MSVC added this in VS2015 (_MSC_VER 1900) but doesn't seem inlined and is very slow.
 * It's slightly faster (~5%) but causes fuzzy PCM<>float<>PCM conversions.
 */
static inline int float_to_int(float val) {
#ifdef PCM16_ROUNDING_LRINT
    return lrintf(val);
#elif defined(_MSC_VER)
    return (int)val;
#else
    return (int)val;
#endif
}

#if 0
static inline int double_to_int(double val) {
#ifdef PCM16_ROUNDING_LRINT
    return lrint(val);
#elif defined(_MSC_VER)
    return (int)val;
#else
    return (int)val;
#endif
}

static inline float double_to_float(double val) {
    return (float)val;
}
#endif

static inline int64_t float_to_i64(float val) {
    return (int64_t)val;
}

// TO-DO: investigate if BE machines need BE 24-bit
static inline int32_t get_s24ne(const uint8_t* p) {
#if PCM24_BIG_ENDIAN
    return (((int32_t)p[0]<<24) | ((int32_t)p[1]<<16) | ((int32_t)p[2]<<8)) >> 8; //force signedness
#else
    return (((int32_t)p[0]<<8) | ((int32_t)p[1]<<16) | ((int32_t)p[2]<<24)) >> 8; //force signedness
#endif
}

static void put_u24ne(uint8_t* buf, uint32_t v) {
#if PCM24_BIG_ENDIAN
    buf[0] = (uint8_t)((v >> 16) & 0xFF);
    buf[1] = (uint8_t)((v >>  8) & 0xFF);
    buf[2] = (uint8_t)((v >>  0) & 0xFF);
#else
    buf[0] = (uint8_t)((v >>  0) & 0xFF);
    buf[1] = (uint8_t)((v >>  8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
#endif
}

static inline int clamp_pcm16(int32_t val) {
    return clamp16(val);
}

static inline int clamp_pcm24(int32_t val) {
    if (val > 8388607) return 8388607;
    else if (val < -8388608) return -8388608;
    else return val;
}

static inline int clamp_pcm32(int64_t val) {
    if (val > 2147483647) return 2147483647;
    else if (val < (-2147483647 - 1)) return (-2147483647 - 1);
    else return val;
}

//TODO float can't fully represent s32, but since it mainly affects lower bytes (noise) it may be ok

#define CONV_NOOP(x) (x)
#define CONV_S16_FLT(x) (x * (1.0f / 32767.0f))
#define CONV_S16_S24(x) (x << 8)
#define CONV_S16_S32(x) (x << 16)

#define CONV_F16_S16(x) (clamp_pcm16(float_to_int(x)))
#define CONV_F16_FLT(x) (x * (1.0f / 32767.0f))
#define CONV_F16_S24(x) (clamp_pcm16(float_to_int(x)) << 8)
#define CONV_F16_S32(x) (clamp_pcm16(float_to_int(x)) << 16)

#ifdef PCM16_ROUNDING_HALF
#define CONV_FLT_S16(x) (clamp_pcm16(float_to_int( floor(x * 32767.0f + 0.5f) )))
#else
#define CONV_FLT_S16(x) (clamp_pcm16(float_to_int(x * 32767.0f)))
#endif
#define CONV_FLT_F16(x) (x * 32767.0f)
#define CONV_FLT_S24(x) (clamp_pcm24(float_to_int(x * 8388607.0f)))
#define CONV_FLT_S32(x) (clamp_pcm32(float_to_i64(x * 2147483647)))

#define CONV_S24_S16(x) (x >> 8)
#define CONV_S24_F16(x) (x >> 8)
#define CONV_S24_S32(x) (x << 8)
#define CONV_S24_FLT(x) (x * (1.0f / 8388607.0f))

#define CONV_S32_S16(x) (x >> 16)
#define CONV_S32_F16(x) (x >> 16)
#define CONV_S32_FLT(x) (x * (1.0f / 2147483647.0f))
#define CONV_S32_S24(x) (x >> 8)


void sbuf_init(sbuf_t* sbuf, sfmt_t format, void* buf, int samples, int channels) {
    memset(sbuf, 0, sizeof(sbuf_t));
    sbuf->buf = buf;
    sbuf->samples = samples;
    sbuf->channels = channels;
    sbuf->fmt = format;
}

void sbuf_init_s16(sbuf_t* sbuf, int16_t* buf, int samples, int channels) {
    sbuf_init(sbuf, SFMT_S16, buf, samples, channels);
}

void sbuf_init_f16(sbuf_t* sbuf, float* buf, int samples, int channels) {
    sbuf_init(sbuf, SFMT_F16, buf, samples, channels);
}

void sbuf_init_flt(sbuf_t* sbuf, float* buf, int samples, int channels) {
    sbuf_init(sbuf, SFMT_FLT, buf, samples, channels);
}


int sfmt_get_sample_size(sfmt_t fmt) {
    switch(fmt) {
        case SFMT_F16:
        case SFMT_FLT:
        case SFMT_S24:
        case SFMT_S32:
            return 0x04;
        case SFMT_S16:
            return 0x02;
        case SFMT_O24:
           return 0x03;
        default:
            VGM_LOG_ONCE("SBUF: undefined sample format %i found\n", fmt);
            return 0; //TODO return 4 to avoid crashes?
    }
}

void* sbuf_get_filled_buf(sbuf_t* sbuf) {
    int sample_size = sfmt_get_sample_size(sbuf->fmt);

    uint8_t* buf = sbuf->buf;
    buf += sbuf->filled * sbuf->channels * sample_size;
    return buf;
}

void sbuf_consume(sbuf_t* sbuf, int samples) {
    if (samples == 0) //some discards
        return;
    int sample_size = sfmt_get_sample_size(sbuf->fmt);
    if (sample_size <= 0) //???
        return;
    if (samples > sbuf->samples || samples > sbuf->filled) //???
        return;

    uint8_t* buf = sbuf->buf;
    buf += samples * sbuf->channels * sample_size;

    sbuf->buf = buf;
    sbuf->filled -= samples;
    sbuf->samples -= samples;
}


// max samples to copy from ssrc to sdst, considering that dst may be partially filled
int sbuf_get_copy_max(sbuf_t* sdst, sbuf_t* ssrc) {
    int sdst_max = sdst->samples - sdst->filled;
    int samples_copy = ssrc->filled;
    if (samples_copy > sdst_max)
        samples_copy = sdst_max;
    return samples_copy;
}

void sbuf_silence_part(sbuf_t* sbuf, int from, int count) {
    int sample_size = sfmt_get_sample_size(sbuf->fmt);

    uint8_t* buf = sbuf->buf;
    buf += from * sbuf->channels * sample_size;
    memset(buf, 0, count * sbuf->channels * sample_size);
}

void sbuf_silence_rest(sbuf_t* sbuf) {
    sbuf_silence_part(sbuf, sbuf->filled, sbuf->samples - sbuf->filled);
}



typedef void (*sbuf_copy_t)(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max);

// Ugly generic copy function definition with different parameters, to avoid repeating code.
// Must define various functions below, to be set in the copy matrix.
// Uses void params to allow callbacks.
#define DEFINE_SBUF_COPY(suffix, srctype, dsttype, func) \
    static void sbuf_copy_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max) { \
        srctype* src = vsrc; \
        dsttype* dst = vdst; \
        while (src_pos < src_max) { \
            dst[dst_pos++] = func(src[src_pos++]); \
        } \
    }

#define DEFINE_SBUF_CP24(suffix, srctype, dsttype, func) \
    static void sbuf_copy_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max) { \
        srctype* src = vsrc; \
        dsttype* dst = vdst; \
        while (src_pos < src_max) { \
            put_u24ne(dst + dst_pos, func(src[src_pos++]) ); \
            dst_pos += 3; \
        } \
    }

DEFINE_SBUF_COPY(s16_s16, int16_t, int16_t, CONV_NOOP);
DEFINE_SBUF_COPY(s16_f16, int16_t, float,   CONV_NOOP);
DEFINE_SBUF_COPY(s16_flt, int16_t, float,   CONV_S16_FLT);
DEFINE_SBUF_COPY(s16_s24, int16_t, int32_t, CONV_S16_S24);
DEFINE_SBUF_COPY(s16_s32, int16_t, int32_t, CONV_S16_S32);
DEFINE_SBUF_CP24(s16_o24, int16_t, uint8_t, CONV_S16_S24);

DEFINE_SBUF_COPY(f16_s16, float,   int16_t, CONV_F16_S16);
DEFINE_SBUF_COPY(f16_f16, float,   float,   CONV_NOOP);
DEFINE_SBUF_COPY(f16_flt, float,   float,   CONV_F16_FLT);
DEFINE_SBUF_COPY(f16_s24, float,   int32_t, CONV_F16_S24);
DEFINE_SBUF_COPY(f16_s32, float,   int32_t, CONV_F16_S32);
DEFINE_SBUF_CP24(f16_o24, float,   uint8_t, CONV_F16_S24);

DEFINE_SBUF_COPY(flt_s16, float,   int16_t, CONV_FLT_S16);
DEFINE_SBUF_COPY(flt_f16, float,   float,   CONV_FLT_F16);
DEFINE_SBUF_COPY(flt_flt, float,   float,   CONV_NOOP);
DEFINE_SBUF_COPY(flt_s24, float,   int32_t, CONV_FLT_S24);
DEFINE_SBUF_COPY(flt_s32, float,   int32_t, CONV_FLT_S32);
DEFINE_SBUF_CP24(flt_o24, float,   uint8_t, CONV_FLT_S24);

DEFINE_SBUF_COPY(s24_s16, int32_t, int16_t, CONV_S24_S16);
DEFINE_SBUF_COPY(s24_f16, int32_t, float,   CONV_S24_F16);
DEFINE_SBUF_COPY(s24_flt, int32_t, float,   CONV_S24_FLT);
DEFINE_SBUF_COPY(s24_s24, int32_t, int32_t, CONV_NOOP);
DEFINE_SBUF_COPY(s24_s32, int32_t, int32_t, CONV_S24_S32);
DEFINE_SBUF_CP24(s24_o24, int32_t, uint8_t, CONV_NOOP);

DEFINE_SBUF_COPY(s32_s16, int32_t, int16_t, CONV_S32_S16);
DEFINE_SBUF_COPY(s32_f16, int32_t, float,   CONV_S32_F16);
DEFINE_SBUF_COPY(s32_flt, int32_t, float,   CONV_S32_FLT);
DEFINE_SBUF_COPY(s32_s24, int32_t, int32_t, CONV_S32_S24);
DEFINE_SBUF_COPY(s32_s32, int32_t, int32_t, CONV_NOOP);
DEFINE_SBUF_CP24(s32_o24, int32_t, uint8_t, CONV_S32_S24);

static sbuf_copy_t copy_matrix[SFMT_MAX][SFMT_MAX] = {
    { NULL, NULL, NULL, NULL, NULL }, //NONE
    { NULL, sbuf_copy_s16_s16, sbuf_copy_s16_f16, sbuf_copy_s16_flt, sbuf_copy_s16_s24, sbuf_copy_s16_s32, sbuf_copy_s16_o24 },
    { NULL, sbuf_copy_f16_s16, sbuf_copy_f16_f16, sbuf_copy_f16_flt, sbuf_copy_f16_s24, sbuf_copy_f16_s32, sbuf_copy_f16_o24 },
    { NULL, sbuf_copy_flt_s16, sbuf_copy_flt_f16, sbuf_copy_flt_flt, sbuf_copy_flt_s24, sbuf_copy_flt_s32, sbuf_copy_flt_o24 },
    { NULL, sbuf_copy_s24_s16, sbuf_copy_s24_f16, sbuf_copy_s24_flt, sbuf_copy_s24_s24, sbuf_copy_s24_s32, sbuf_copy_s24_o24 },
    { NULL, sbuf_copy_s32_s16, sbuf_copy_s32_f16, sbuf_copy_s32_flt, sbuf_copy_s32_s24, sbuf_copy_s32_s32, sbuf_copy_s32_o24 },
    { NULL, NULL, NULL, NULL, NULL }, //O24
};


// copy N samples from ssrc into dst (should be clamped externally)
//TODO: may want to handle sdst->flled + samples externally?
void sbuf_copy_segments(sbuf_t* sdst, sbuf_t* ssrc, int samples) {
    // rarely when decoding with empty frames, may not setup ssrc
    if (samples == 0)
        return;

    if (ssrc->channels != sdst->channels) {
        // 0'd other channels first (uncommon so probably fine albeit slower-ish)
        sbuf_silence_part(sdst, sdst->filled, samples);
        sbuf_copy_layers(sdst, ssrc, 0, samples);
#if 0
        // "faster" but lots of extra ifs per sample format, not worth it
        while (src_pos < src_max) {
            for (int ch = 0; ch < dst_channels; ch++) {
                dst[dst_pos++] = ch >= src_channels ? 0 : src[src_pos++];
            }
        }
#endif
        sdst->filled += samples;
        return;
    }

    sbuf_copy_t sbuf_copy_src_dst = copy_matrix[ssrc->fmt][sdst->fmt];
    if (!sbuf_copy_src_dst) {
        VGM_LOG("SBUF: undefined copy function sfmt %i to %i\n", ssrc->fmt, sdst->fmt);
        sdst->filled += samples;
        return;
    }

    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels;
    int src_max = samples * ssrc->channels;

    sbuf_copy_src_dst(ssrc->buf, sdst->buf, src_pos, dst_pos, src_max);
    sdst->filled += samples;
}

typedef void (*sbuf_layer_t)(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max, int dst_expected, int src_channels, int dst_channels);

// See above
#define DEFINE_SBUF_LAYER(suffix, srctype, dsttype, func) \
    static void sbuf_layer_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_filled, int dst_expected, int src_channels, int dst_channels) { \
        srctype* src = vsrc; \
        dsttype* dst = vdst; \
        int dst_ch_step = (dst_channels - src_channels); \
        for (int s = 0; s < src_filled; s++) { \
            for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
                dst[dst_pos++] = func(src[src_pos++]); \
            } \
            dst_pos += dst_ch_step; \
        } \
        \
        for (int s = src_filled; s < dst_expected; s++) { \
            for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
                dst[dst_pos++] = 0; \
            } \
            dst_pos += dst_ch_step; \
        } \
    }

#define DEFINE_SBUF_LYR24(suffix, srctype, dsttype, func) \
    static void sbuf_layer_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_filled, int dst_expected, int src_channels, int dst_channels) { \
        srctype* src = vsrc; \
        dsttype* dst = vdst; \
        int dst_ch_step = (dst_channels - src_channels); \
        for (int s = 0; s < src_filled; s++) { \
            for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
                put_u24ne(dst + dst_pos, func(src[src_pos++]) ); \
                dst_pos += 3; \
            } \
            dst_pos += dst_ch_step * 3; \
        } \
        \
        for (int s = src_filled; s < dst_expected; s++) { \
            for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
                put_u24ne(dst + dst_pos, 0); \
                dst_pos += 3; \
            } \
            dst_pos += dst_ch_step * 3; \
        } \
    }

DEFINE_SBUF_LAYER(s16_s16, int16_t, int16_t, CONV_NOOP);
DEFINE_SBUF_LAYER(s16_f16, int16_t, float,   CONV_NOOP);
DEFINE_SBUF_LAYER(s16_flt, int16_t, float,   CONV_S16_FLT);
DEFINE_SBUF_LAYER(s16_s24, int16_t, int32_t, CONV_S16_S24);
DEFINE_SBUF_LAYER(s16_s32, int16_t, int32_t, CONV_S16_S32);
DEFINE_SBUF_LYR24(s16_o24, int16_t, uint8_t, CONV_S16_S24);

DEFINE_SBUF_LAYER(f16_s16, float,   int16_t, CONV_F16_S16);
DEFINE_SBUF_LAYER(f16_f16, float,   float,   CONV_NOOP);
DEFINE_SBUF_LAYER(f16_flt, float,   float,   CONV_F16_FLT);
DEFINE_SBUF_LAYER(f16_s24, float,   int32_t, CONV_F16_S24);
DEFINE_SBUF_LAYER(f16_s32, float,   int32_t, CONV_F16_S32);
DEFINE_SBUF_LYR24(f16_o24, float,   uint8_t, CONV_F16_S24);

DEFINE_SBUF_LAYER(flt_s16, float,   int16_t, CONV_FLT_S16);
DEFINE_SBUF_LAYER(flt_f16, float,   float,   CONV_FLT_F16);
DEFINE_SBUF_LAYER(flt_flt, float,   float,   CONV_NOOP);
DEFINE_SBUF_LAYER(flt_s24, float,   int32_t, CONV_FLT_S24);
DEFINE_SBUF_LAYER(flt_s32, float,   int32_t, CONV_FLT_S32);
DEFINE_SBUF_LYR24(flt_o24, float,   uint8_t, CONV_FLT_S24);

DEFINE_SBUF_LAYER(s24_s16, int32_t, int16_t, CONV_S24_S16);
DEFINE_SBUF_LAYER(s24_f16, int32_t, float,   CONV_S24_F16);
DEFINE_SBUF_LAYER(s24_flt, int32_t, float,   CONV_S24_FLT);
DEFINE_SBUF_LAYER(s24_s24, int32_t, int32_t, CONV_NOOP);
DEFINE_SBUF_LAYER(s24_s32, int32_t, int32_t, CONV_S24_S32);
DEFINE_SBUF_LYR24(s24_o24, int32_t, uint8_t, CONV_NOOP);

DEFINE_SBUF_LAYER(s32_s16, int32_t, int16_t, CONV_S32_S16);
DEFINE_SBUF_LAYER(s32_f16, int32_t, float,   CONV_S32_F16);
DEFINE_SBUF_LAYER(s32_flt, int32_t, float,   CONV_S32_FLT);
DEFINE_SBUF_LAYER(s32_s24, int32_t, int32_t, CONV_S32_S24);
DEFINE_SBUF_LAYER(s32_s32, int32_t, int32_t, CONV_NOOP);
DEFINE_SBUF_LYR24(s32_o24, int32_t, uint8_t, CONV_NOOP);

static sbuf_layer_t layer_matrix[SFMT_MAX][SFMT_MAX] = {
    { NULL, NULL, NULL, NULL, NULL }, //NONE
    { NULL, sbuf_layer_s16_s16, sbuf_layer_s16_f16, sbuf_layer_s16_flt, sbuf_layer_s16_s24, sbuf_layer_s16_s32, sbuf_layer_s16_o24 },
    { NULL, sbuf_layer_f16_s16, sbuf_layer_f16_f16, sbuf_layer_f16_flt, sbuf_layer_f16_s24, sbuf_layer_f16_s32, sbuf_layer_f16_o24 },
    { NULL, sbuf_layer_flt_s16, sbuf_layer_flt_f16, sbuf_layer_flt_flt, sbuf_layer_flt_s24, sbuf_layer_flt_s32, sbuf_layer_flt_o24 },
    { NULL, sbuf_layer_s24_s16, sbuf_layer_s24_f16, sbuf_layer_s24_flt, sbuf_layer_s24_s24, sbuf_layer_s24_s32, sbuf_layer_s24_o24 },
    { NULL, sbuf_layer_s32_s16, sbuf_layer_s32_f16, sbuf_layer_s32_flt, sbuf_layer_s32_s24, sbuf_layer_s32_s32, sbuf_layer_s32_o24 },
    { NULL, NULL, NULL, NULL, NULL }, //O32
};

// copy interleaving: dst ch1 ch2 ch3 ch4 w/ src ch1 ch2 ch1 ch2 = only fill dst ch1 ch2
// dst_channels == src_channels isn't likely so ignore that optimization (dst must be >= than src).
// dst_ch_start indicates it should write to dst's chN,chN+1,etc
// sometimes one layer has less samples than others and need to 0-fill rest up to dst_max
void sbuf_copy_layers(sbuf_t* sdst, sbuf_t* ssrc, int dst_ch_start, int dst_max) {
    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels + dst_ch_start;

    int src_copy = dst_max;
    if (src_copy > ssrc->filled)
        src_copy = ssrc->filled;

    if (ssrc->channels > sdst->channels) {
        VGM_LOG("SBUF: src channels bigger than dst\n");
        return;
    }

    sbuf_layer_t sbuf_layer_src_dst = layer_matrix[ssrc->fmt][sdst->fmt];
    if (!sbuf_layer_src_dst) {
        VGM_LOG_ONCE("SBUF: undefined layer function sfmt %i to %i\n", ssrc->fmt, sdst->fmt);
        return;
    }

    sbuf_layer_src_dst(ssrc->buf, sdst->buf, src_pos, dst_pos, src_copy, dst_max, ssrc->channels, sdst->channels);
}


typedef void (*sbuf_fade_t)(void* vsrc, int start, int to_do, int fade_pos, int fade_duration);

#define DEFINE_SBUF_FADE(suffix, buftype, func) \
    static void sbuf_fade_##suffix(sbuf_t* sbuf, int start, int to_do, int fade_pos, int fade_duration) { \
        buftype* buf = sbuf->buf; \
        int s = start * sbuf->channels; \
        int s_end = (start + to_do) * sbuf->channels; \
        while (s < s_end) { \
            float fadedness = (float)(fade_duration - fade_pos) / fade_duration; \
            for (int i = 0; i < sbuf->channels; i++) { \
                buf[s] = func(buf[s] * fadedness); \
                s++; \
            } \
            fade_pos++; \
        } \
    }

#define DEFINE_SBUF_FD24(suffix, buftype, func) \
    static void sbuf_fade_##suffix(sbuf_t* sbuf, int start, int to_do, int fade_pos, int fade_duration) { \
        buftype* buf = sbuf->buf; \
        int s = start * sbuf->channels; \
        int s_end = (start + to_do) * sbuf->channels; \
        while (s < s_end) { \
            float fadedness = (float)(fade_duration - fade_pos) / fade_duration; \
            for (int i = 0; i < sbuf->channels; i++) { \
                put_u24ne(buf + s * 3, func(get_s24ne(buf + s * 3) * fadedness) ); \
                s++; \
            } \
            fade_pos++; \
        } \
    }

// no need to clamp in fade outs
#define CONV_FADE_FLT(x) (x)
#define CONV_FADE_PCM(x) float_to_int(x)

DEFINE_SBUF_FADE(i16, int16_t, CONV_FADE_PCM);
DEFINE_SBUF_FADE(i32, int32_t, CONV_FADE_PCM);
DEFINE_SBUF_FADE(flt, float, CONV_FADE_FLT);
DEFINE_SBUF_FD24(o24, uint8_t, CONV_FADE_PCM);

void sbuf_fadeout(sbuf_t* sbuf, int start, int to_do, int fade_pos, int fade_duration) {
    //TODO: use interpolated fadedness to improve performance?
    //TODO: use float fadedness?

    switch(sbuf->fmt) {
        case SFMT_S16:
            sbuf_fade_i16(sbuf, start, to_do, fade_pos, fade_duration);
            break;
        case SFMT_S24:
        case SFMT_S32:
            sbuf_fade_i32(sbuf, start, to_do, fade_pos, fade_duration);
            break;
        case SFMT_FLT:
        case SFMT_F16:
            sbuf_fade_flt(sbuf, start, to_do, fade_pos, fade_duration);
            break;
        case SFMT_O24:
            sbuf_fade_o24(sbuf, start, to_do, fade_pos, fade_duration);
            break;
        default:
            VGM_LOG("SBUF: missing fade for fmt=%i\n", sbuf->fmt);
            break;
    }

    /* next samples after fade end would be pad end/silence */
    int count = sbuf->filled - (start + to_do);
    sbuf_silence_part(sbuf, start + to_do, count);
}

void sbuf_interleave(sbuf_t* sbuf, float** ibuf) {
    if (sbuf->fmt != SFMT_FLT)
        return;

    // copy multidimensional buf (pcm[0]=[ch0,ch0,...], pcm[1]=[ch1,ch1,...])
    // to interleaved buf (buf[0]=ch0, sbuf[1]=ch1, sbuf[2]=ch0, sbuf[3]=ch1, ...)
    for (int ch = 0; ch < sbuf->channels; ch++) {
        /* channels should be in standard order unlike Ogg Vorbis (at least in FSB) */
        float* ptr = sbuf->buf;
        float* channel = ibuf[ch];

        ptr += ch;
        for (int s = 0; s < sbuf->filled; s++) {
            float val = channel[s];
            #if 0 //to pcm16 //from vorbis)
            int val = (int)floor(channel[s] * 32767.0f + 0.5f);
            if (val > 32767) val = 32767;
            else if (val < -32768) val = -32768;
            #endif

            *ptr = val;
            ptr += sbuf->channels;
        }
    }
}

/* vorbis encodes channels in non-standard order, so we remap during conversion to fix this oddity.
 * (feels a bit weird as one would think you could leave as-is and set the player's output order,
 * but that isn't possible and remapping like this is what FFmpeg and every other plugin does). */
static const int xiph_channel_map[8][8] = {
    { 0 },                          // 1ch: FC > same
    { 0, 1 },                       // 2ch: FL FR > same
    { 0, 2, 1 },                    // 3ch: FL FC FR > FL FR FC
    { 0, 1, 2, 3 },                 // 4ch: FL FR BL BR > same
    { 0, 2, 1, 3, 4 },              // 5ch: FL FC FR BL BR > FL FR FC BL BR
    { 0, 2, 1, 5, 3, 4 },           // 6ch: FL FC FR BL BR LFE > FL FR FC LFE BL BR
    { 0, 2, 1, 6, 5, 3, 4 },        // 7ch: FL FC FR SL SR BC LFE > FL FR FC LFE BC SL SR
    { 0, 2, 1, 7, 5, 6, 3, 4 },     // 8ch: FL FC FR SL SR BL BR LFE > FL FR FC LFE BL BR SL SR
};

// converts from internal Vorbis format to standard PCM and remaps (mostly from Xiph's decoder_example.c)
void sbuf_interleave_vorbis(sbuf_t* sbuf, float** src) {
    if (sbuf->fmt != SFMT_FLT)
        return;
    int channels = sbuf->channels;

    /* convert float PCM (multichannel float array, with pcm[0]=ch0, pcm[1]=ch1, pcm[2]=ch0, etc)
     * to 16 bit signed PCM ints (host order) and interleave + fix clipping */
    for (int ch = 0; ch < channels; ch++) {
        int ch_map = (channels > 8) ? ch : xiph_channel_map[channels - 1][ch]; // put Vorbis' ch to other outbuf's ch
        float* ptr = sbuf->buf;
        float* channel = src[ch_map];

        ptr += ch;
        for (int s = 0; s < sbuf->filled; s++) {
            float val = channel[s];

            #if 0 //to pcm16 from vorbis
            int val = (int)floor(channel[s] * 32767.0f + 0.5f);
            if (val > 32767) val = 32767;
            else if (val < -32768) val = -32768;
            #endif

            *ptr = val;
            ptr += channels;
        }
    }
}
