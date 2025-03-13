#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "sbuf.h"
#include "../util/log.h"

// float-to-int modes
//#define PCM16_ROUNDING_LRINT  // potentially faster in some systems/compilers and much slower in others
//#define PCM16_ROUNDING_HALF   // rounding half + down (vorbis-style), more 'accurate' but slower

#ifdef PCM16_ROUNDING_HALF
#include <math.h>
#endif

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

void sbuf_init_f32(sbuf_t* sbuf, float* buf, int samples, int channels) {
    sbuf_init(sbuf, SFMT_F32, buf, samples, channels);
}

void sbuf_init_flt(sbuf_t* sbuf, float* buf, int samples, int channels) {
    sbuf_init(sbuf, SFMT_FLT, buf, samples, channels);
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
#if PCM16_ROUNDING_LRINT
    return lrintf(val);
#elif defined(_MSC_VER)
    return (int)val;
#else
    return (int)val;
#endif
}

static inline int double_to_int(double val) {
#if PCM16_ROUNDING_LRINT
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
        case SFMT_F32: {
            float* src = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = src[s];
            }
            break;
        }
        case SFMT_FLT: {
            float* src = sbuf->buf;
            for (int s = 0; s < sbuf->filled * sbuf->channels; s++) {
                dst[s] = src[s] * 32767.0f;
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
                dst[s] = src[s] / 32767.0f;
            }
            break;
        }
        default:
            break;
    }
}

// max samples to copy from ssrc to sdst, considering that dst may be partially filled
int sbuf_get_copy_max(sbuf_t* sdst, sbuf_t* ssrc) {
    int sdst_max = sdst->samples - sdst->filled;
    int samples_copy = ssrc->filled;
    if (samples_copy > sdst_max)
        samples_copy = sdst_max;
    return samples_copy;
}


/* ugly thing to avoid repeating functions */
#define sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = src[src_pos++]; \
    }

#define sbuf_copy_segments_internal_f16(dst, src, src_pos, dst_pos, src_max) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = clamp16(float_to_int(src[src_pos++])); \
    }

#ifdef PCM16_ROUNDING_HALF
#define sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, value) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = clamp16(float_to_int( floor(src[src_pos++] * value + 0.5f) )); \
    }
#else
#define sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, value) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = clamp16(float_to_int(src[src_pos++] * value)); \
    }
#endif

#define sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, value) \
    while (src_pos < src_max) { \
        dst[dst_pos++] = (src[src_pos++] * value); \
    }

// copy N samples from ssrc into dst (should be clamped externally)
void sbuf_copy_segments(sbuf_t* sdst, sbuf_t* ssrc, int samples_copy) {

    if (ssrc->channels != sdst->channels) {
        // 0'd other channels first (uncommon so probably fine albeit slower-ish)
        sbuf_silence_part(sdst, sdst->filled, samples_copy);
        sbuf_copy_layers(sdst, ssrc, 0, samples_copy);
#if 0
        // "faster" but lots of extra ifs per sample format, not worth it
        while (src_pos < src_max) {
            for (int ch = 0; ch < dst_channels; ch++) {
                dst[dst_pos++] = ch >= src_channels ? 0 : src[src_pos++];
            }
        }
#endif
        //TODO: may want to handle externally?
        sdst->filled += samples_copy;
        return;
    }

    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels;
    int src_max = samples_copy * ssrc->channels;

    // define all posible combos, probably there is a better way to handle this but...
    // s16 > s16
    if (ssrc->fmt == SFMT_S16 && sdst->fmt == SFMT_S16) {
        int16_t* src = ssrc->buf;
        int16_t* dst = sdst->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    // s16 > f32
    else if (ssrc->fmt == SFMT_S16 && sdst->fmt == SFMT_F32) {
        int16_t* src = ssrc->buf;
        float* dst = sdst->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    // s16 > flt
    else if (ssrc->fmt == SFMT_S16 && sdst->fmt == SFMT_FLT) {
        int16_t* src = ssrc->buf;
        float* dst = sdst->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, (1.0f / 32767.0f));
    }
    // f32 > f32 / flt > flt
    else if ((ssrc->fmt == SFMT_F32 && sdst->fmt == SFMT_F32) ||
             (ssrc->fmt == SFMT_FLT && sdst->fmt == SFMT_FLT)) {
        float* src = ssrc->buf;
        float* dst = sdst->buf;
        sbuf_copy_segments_internal(dst, src, src_pos, dst_pos, src_max);
    }
    // f32 > s16
    else if (ssrc->fmt == SFMT_F32 && sdst->fmt == SFMT_S16) {
        float* src = ssrc->buf;
        int16_t* dst = sdst->buf;
        sbuf_copy_segments_internal_f16(dst, src, src_pos, dst_pos, src_max);
    }
    // flt > s16
    else if (ssrc->fmt == SFMT_FLT && sdst->fmt == SFMT_S16) {
        float* src = ssrc->buf;
        int16_t* dst = sdst->buf;
        sbuf_copy_segments_internal_s16(dst, src, src_pos, dst_pos, src_max, 32767.0f);
    }
    // f32 > flt
    else if (ssrc->fmt == SFMT_F32 && sdst->fmt == SFMT_FLT) {
        float* src = ssrc->buf;
        float* dst = sdst->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, (1.0f / 32767.0f));
    }
    // flt > f32
    else if (ssrc->fmt == SFMT_FLT && sdst->fmt == SFMT_F32) {
        float* src = ssrc->buf;
        float* dst = sdst->buf;
        sbuf_copy_segments_internal_flt(dst, src, src_pos, dst_pos, src_max, 32767.0f);
    }

    //TODO: may want to handle externally?
    sdst->filled += samples_copy;
}


#define sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step) \
    for (int s = src_filled; s < dst_expected; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = 0; \
        } \
        dst_pos += dst_ch_step; \
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
    sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);

// float +-1.0 <> pcm +-32767.0
#define sbuf_copy_layers_internal_f16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = clamp16(float_to_int(src[src_pos++])); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);

#ifdef PCM16_ROUNDING_HALF
// float +-1.0 <> pcm +-32767.0
#define sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, value) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = clamp16(float_to_int( floor(src[src_pos++] * value + 0.5f) )); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);
#else
// float +-1.0 <> pcm +-32767.0
#define sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, value) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = clamp16(float_to_int(src[src_pos++] * value)); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);
#endif

// float +-1.0 <> pcm +-32767.0
#define sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step, value) \
    for (int s = 0; s < src_filled; s++) { \
        for (int src_ch = 0; src_ch < src_channels; src_ch++) { \
            dst[dst_pos++] = float_to_int(src[src_pos++] * value); \
        } \
        dst_pos += dst_ch_step; \
    } \
    \
    sbuf_copy_layers_internal_blank(dst, src, src_pos, dst_pos, src_filled, dst_expected, src_channels, dst_ch_step);

// copy interleaving: dst ch1 ch2 ch3 ch4 w/ src ch1 ch2 ch1 ch2 = only fill dst ch1 ch2
// dst_channels == src_channels isn't likely so ignore that optimization (dst must be >= than src).
// dst_ch_start indicates it should write to dst's chN,chN+1,etc
// sometimes one layer has less samples than others and need to 0-fill rest
void sbuf_copy_layers(sbuf_t* sdst, sbuf_t* ssrc, int dst_ch_start, int dst_max) {
    int src_copy = dst_max;
    int src_channels = ssrc->channels;
    int dst_ch_step = (sdst->channels - ssrc->channels);
    int src_pos = 0;
    int dst_pos = sdst->filled * sdst->channels + dst_ch_start;

    if (src_copy > ssrc->filled)
        src_copy = ssrc->filled;

    if (ssrc->channels > sdst->channels) {
        VGM_LOG("SBUF: wrong copy\n");
        return;
    }

    // define all posible combos, probably there is a better way to handle this but...

    // 1:1
    if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_S16) {
        int16_t* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step);
    }
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step);
    }
    else if ((sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_F32) || (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_FLT)) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step);
    }
    // to s16
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_F32) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_f16(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step);
    }
    else if (sdst->fmt == SFMT_S16 && ssrc->fmt == SFMT_FLT) {
        int16_t* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_s16(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step, 32767.0f);
    }
    // to f32
    else if (sdst->fmt == SFMT_F32 && ssrc->fmt == SFMT_FLT) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step, 32767.0f);
    }
    // to flt
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_S16) {
        float* dst = sdst->buf;
        int16_t* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step, (1.0f / 32767.0f));
    }
    else if (sdst->fmt == SFMT_FLT && ssrc->fmt == SFMT_F32) {
        float* dst = sdst->buf;
        float* src = ssrc->buf;
        sbuf_copy_layers_internal_flt(dst, src, src_pos, dst_pos, src_copy, dst_max, src_channels, dst_ch_step, (1.0f / 32767.0f));
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
