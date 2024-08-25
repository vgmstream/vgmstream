#include <stdlib.h>
#include <string.h>
//#include <math.h>
#include "../util.h"
#include "sbuf.h"


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

void sbuf_copy_samples(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled) {
    int pos = samples_filled * dst_channels;

    if (src_channels == dst_channels) { /* most common and probably faster */
        for (int s = 0; s < samples_to_do * dst_channels; s++) {
            dst[pos + s] = src[s];
        }
    }
    else {
        for (int s = 0; s < samples_to_do; s++) {
            for (int ch = 0; ch < src_channels; ch++) {
                dst[pos + s * dst_channels + ch] = src[s * src_channels + ch];
            }
            for (int ch = src_channels; ch < dst_channels; ch++) {
                dst[pos + s * dst_channels + ch] = 0;
            }
        }
    }
}

/* copy interleaving */
void sbuf_copy_layers(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled, int dst_ch_start) {
    // dst_channels == src_channels isn't likely
    for (int src_ch = 0; src_ch < src_channels; src_ch++) {
        for (int s = 0; s < samples_to_do; s++) {
            int src_pos = s * src_channels + src_ch;
            int dst_pos = (samples_filled + s) * dst_channels + dst_ch_start;

            dst[dst_pos] = src[src_pos];
        }

        dst_ch_start++;
    }
}

void sbuf_silence_s16(sample_t* dst, int samples, int channels, int filled) {
    memset(dst + filled * channels, 0, (samples - filled) * channels * sizeof(sample_t));   
}

bool sbuf_realloc(sample_t** dst, int samples, int channels) {
    sample_t* outbuf_re = realloc(*dst, samples * channels * sizeof(sample_t));
    if (!outbuf_re) return false;

    *dst = outbuf_re;
    return true;
}
