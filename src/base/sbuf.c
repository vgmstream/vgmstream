#include <stdlib.h>
#include <string.h>
//#include <math.h>
#include "../util.h"
#include "sbuf.h"


void sbuf_init(sbuf_t* sbuf, sfmt_t format, void* buf, int samples, int channels) {
    memset(sbuf, 0, sizeof(sbuf_t));
    sbuf->buf = buf;
    sbuf->samples = samples;
    sbuf->channels = channels;
    sbuf->fmt = format;
}

void sbuf_init_s16(sbuf_t* sbuf, int16_t* buf, int samples, int channels) {
    memset(sbuf, 0, sizeof(sbuf_t));
    sbuf->buf = buf;
    sbuf->samples = samples;
    sbuf->channels = channels;
    sbuf->fmt = SFMT_S16;
}

void sbuf_init_f32(sbuf_t* sbuf, float* buf, int samples, int channels) {
    memset(sbuf, 0, sizeof(sbuf_t));
    sbuf->buf = buf;
    sbuf->samples = samples;
    sbuf->channels = channels;
    sbuf->fmt = SFMT_F32;
}


int sfmt_get_sample_size(sfmt_t fmt) {
    switch(fmt) {
        case SFMT_F32:
        case SFMT_FLT:
            return 0x04;
        case SFMT_S16:
            return 0x02;
        default:
            return 0;
    }
}

void* sbuf_get_filled_buf(sbuf_t* sbuf) {
    int sample_size = sfmt_get_sample_size(sbuf->fmt);

    uint8_t* buf = sbuf->buf;
    buf += sbuf->filled * sbuf->channels * sample_size;
    return buf;
}

void sbuf_consume(sbuf_t* sbuf, int count) {
    int sample_size = sfmt_get_sample_size(sbuf->fmt);
    if (sample_size <= 0)
        return;
    if (count > sbuf->samples || count > sbuf->filled) //TODO?
        return;

    uint8_t* buf = sbuf->buf;
    buf += count * sbuf->channels * sample_size;

    sbuf->buf = buf;
    sbuf->filled -= count;
    sbuf->samples -= count;
}

/* when casting float to int, value is simply truncated:
 * - (int)1.7 = 1, (int)-1.7 = -1
 * alts for more accurate rounding could be:
 * - (int)floor(f)
 * - (int)(f < 0 ? f - 0.5f : f + 0.5f)
 * - (((int) (f1 + 32768.5)) - 32768)
 * - etc
 * but since +-1 isn't really audible we'll just cast, as it's the fastest
 *
 * Regular C float-to-int casting ("int i = (int)f") is somewhat slow due to IEEE
 * float requirements, but C99 adds some faster-but-less-precise casting functions.
 * MSVC added this in VS2015 (_MSC_VER 1900) but doesn't seem inlined and is very slow.
 * It's slightly faster (~5%) but causes fuzzy PCM<>float<>PCM conversions.
 */
static inline int float_to_int(float val) {
#if 1
    return (int)val;
#elif defined(_MSC_VER)
    return (int)val;
#else
    return lrintf(val);
#endif
}

static inline int double_to_int(double val) {
#if 1
    return (int)val;
#elif defined(_MSC_VER)
    return (int)val;
#else
    return lrint(val);
#endif
}

static inline float double_to_float(double val) {
    return (float)val;
}

//TODO decide if using float 1.0 style or 32767 style (fuzzy PCM when doing that)
//TODO: maybe use macro-style templating (but kinda ugly)
void sbuf_copy_to_f32(float* dst, sbuf_t* sbuf) {

    switch(sbuf->fmt) {
        case SFMT_S16: {
            int16_t* src = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = (float)src[s]; // / 32767.0f
            }
            break;
        }

        case SFMT_FLT:
        case SFMT_F32: {
            float* src = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = src[s];
            }
            break;
        }
        default:
            break;
    }
}

void sbuf_copy_from_f32(sbuf_t* sbuf, float* src) {
    switch(sbuf->fmt) {
        case SFMT_S16: {
            int16_t* dst = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = clamp16(float_to_int(src[s]));
            }
            break;
        }
        case SFMT_F32: {
            float* dst = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = src[s];
            }
            break;
        }
        case SFMT_FLT: {
            float* dst = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = src[s] / 32768.0f;
            }
            break;
        }
        default:
            break;
    }
}


/* ugly thing to avoid repeating functions */
#define sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = src[src_pos++]; \
    }

#define sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, value) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = clamp16(float_to_int(src[src_pos++] * value)); \
    }

#define sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, value) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = float_to_int(src[src_pos++] * value); \
    }

void sbuf_copy_segments(sbuf_t* sdst, sbuf_t* ssrc) {
    /* uncommon so probably fine albeit slower-ish, 0'd other channels first */
    if (ssrc->channels != sdst->channels) {
        sbuf_silence_part(sdst, sdst->filled, ssrc->filled);
        sbuf_copy_layers(sdst, ssrc, 0, ssrc->filled);
#if 0
        // "faster" but lots of extra ifs, not worth it
        while (src_pos < src_max) {
            for (int ch = 0; ch < dst_channels; ch++) {
                dst[dst_pos++] = ch >= src_channels ? 0 : src[src_pos++];
            }
        }
#endif
        return;
    }

    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels;
    int src_max = ssrc->filled * ssrc->channels;

    // define all posible combos, probably there is a better way to handle this but...

    if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_S16) {
        int16_t* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    else if ((sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_F32) || (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_FLT)) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    // to s16
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_F32) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, 1.0f);
    }
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_FLT) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, 32768.0f);
    }
    // to f32
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_FLT) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, 32768.0f);
    }
    // to flt
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, (1/32768.0f));
    }
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_F32) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, (1/32768.0f));
    }
}


//TODO fix missing ->channels
/* ugly thing to avoid repeating functions */
#define sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = src[src_pos++]; \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    for (int s = src_filled; s < dst_expected; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = 0; \
        } \
        dst_pos += dst_ch_step; \
    }

// float +-1.0 <> pcm +-32768.0
#define sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, value) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = clamp16(float_to_int(src[src_pos++] * value)); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    for (int s = src_filled; s < dst_expected; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = 0; \
        } \
        dst_pos += dst_ch_step; \
    }

// float +-1.0 <> pcm +-32768.0
#define sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, value) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = float_to_int(src[src_pos++] * value); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    for (int s = src_filled; s < dst_expected; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = 0; \
        } \
        dst_pos += dst_ch_step; \
    }

/* copy interleaving: dst ch1 ch2 ch3 ch4 w/ src ch1 ch2 ch1 ch2 = only fill dst ch1 ch2 */
// dst_channels == src_channels isn't likely so ignore that optimization
// sometimes one layer has less samples than others and need to 0-fill rest
void sbuf_copy_layers(sbuf_t* sdst, sbuf_t* ssrc, int dst_ch_start, int dst_expected) {
    int src_filled = ssrc->filled;
    int src_channels = ssrc->channels;
    int dst_ch_step = (sdst->channels - ssrc->channels); \
    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels + dst_ch_start;

    // define all posible combos, probably there is a better way to handle this but...

    // 1:1
    if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_S16) {
        int16_t* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);
    }
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);
    }
    else if ((sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_F32) || (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_FLT)) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);
    }
    // to s16
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_F32) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, 1.0f);
    }
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_FLT) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, 32768.0f);
    }
    // to f32
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_FLT) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, 32768.0f);
    }
    // to flt
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, (1/32768.0f));
    }
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_F32) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, (1/32768.0f));
    }
}

void sbuf_silence_s16(sample_t* dst, int samples, int channels, int filled) {
    memset(dst + filled * channels, 0, (samples - filled) * channels * sizeof(sample_t));
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

void sbuf_fadeout(sbuf_t* sbuf, int start, int to_do, int fade_pos, int fade_duration) {
    //TODO: use interpolated fadedness to improve performance?
    //TODO: use float fadedness?
    
    int s = start * sbuf->channels;
    int s_end = (start + to_do) * sbuf->channels;
            

    switch(sbuf->fmt) {
        case SFMT_S16: {
            int16_t* buf = sbuf->buf;
            while (s < s_end) {
                double fadedness = (double)(fade_duration - fade_pos) / fade_duration;
                fade_pos++;

                for (int ch = 0; ch < sbuf->channels; ch++) {
                    buf[s] = double_to_int(buf[s] * fadedness);
                    s++;
                }
            }
            break;
        }

        case SFMT_FLT:
        case SFMT_F32: {
            float* buf = sbuf->buf;
            while (s < s_end) {
                double fadedness = (double)(fade_duration - fade_pos) / fade_duration;
                fade_pos++;

                for (int ch = 0; ch < sbuf->channels; ch++) {
                    buf[s] = double_to_float(buf[s] * fadedness);
                    s++;
                }
            }
            break;
        }
        default:
            break;
    }

    /* next samples after fade end would be pad end/silence */
    int count = sbuf->filled - (start + to_do);
    sbuf_silence_part(sbuf, start + to_do, count);
}
