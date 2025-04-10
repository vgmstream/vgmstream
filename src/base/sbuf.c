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

#define CONV_NOOP(x) (x)
#define CONV_S16_FLT(x) (x * (1.0f / 32767.0f))
#define CONV_F16_FLT(x) (x * (1.0f / 32767.0f))
#ifdef PCM16_ROUNDING_HALF
#define CONV_FLT_S16(x) (clamp16(float_to_int( floor(x * 32767.0f + 0.5f) )))
#else
#define CONV_FLT_S16(x) (clamp16(float_to_int(x * 32767.0f)))
#endif
#define CONV_F16_S16(x) (clamp16(float_to_int(x)))
#define CONV_FLT_F16(x) (x * 32767.0f)


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

// max samples to copy from ssrc to sdst, considering that dst may be partially filled
int sbuf_get_copy_max(sbuf_t* sdst, sbuf_t* ssrc) {
    int sdst_max = sdst->samples - sdst->filled;
    int samples_copy = ssrc->filled;
    if (samples_copy > sdst_max)
        samples_copy = sdst_max;
    return samples_copy;
}


typedef void (*sbuf_copy_t)(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max);

// Ugly generic copy function definition with different parameters, to avoid repeating code.
// Must define various functions below, to be set in the copy matrix.
// Uses void params to allow callbacks.
#define DEFINE_SBUF_COPY(suffix, srctype, dsttype, func) \
    void sbuf_copy_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_max) { \
        srctype* src = vsrc;\
        dsttype* dst = vdst;\
        while (src_pos < src_max) { \
            dst[dst_pos++] = func(src[src_pos++]); \
        } \
    }

DEFINE_SBUF_COPY(s16_s16, int16_t, int16_t, CONV_NOOP);
DEFINE_SBUF_COPY(s16_f16, int16_t, float, CONV_NOOP);
DEFINE_SBUF_COPY(s16_flt, int16_t, float, CONV_S16_FLT);

DEFINE_SBUF_COPY(flt_s16, float, int16_t, CONV_FLT_S16);
DEFINE_SBUF_COPY(flt_f16, float, float, CONV_FLT_F16);
DEFINE_SBUF_COPY(flt_flt, float, float, CONV_NOOP);

DEFINE_SBUF_COPY(f16_s16, float, int16_t, CONV_F16_S16);
DEFINE_SBUF_COPY(f16_f16, float, float, CONV_NOOP);
DEFINE_SBUF_COPY(f16_flt, float, float, CONV_F16_FLT);

static sbuf_copy_t copy_matrix[SFMT_MAX][SFMT_MAX] = {
    { NULL, NULL, NULL, NULL },
    { NULL, sbuf_copy_s16_s16, sbuf_copy_s16_f16, sbuf_copy_s16_flt },
    { NULL, sbuf_copy_f16_s16, sbuf_copy_f16_f16, sbuf_copy_f16_flt },
    { NULL, sbuf_copy_flt_s16, sbuf_copy_flt_f16, sbuf_copy_flt_flt },
};


// copy N samples from ssrc into dst (should be clamped externally)
//TODO: may want to handle sdst->flled + samples externally?
void sbuf_copy_segments(sbuf_t* sdst, sbuf_t* ssrc, int samples) {

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
    void sbuf_layer_##suffix(void* vsrc, void* vdst, int src_pos, int dst_pos, int src_filled, int dst_expected, int src_channels, int dst_channels) { \
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

DEFINE_SBUF_LAYER(s16_s16, int16_t, int16_t,CONV_NOOP);
DEFINE_SBUF_LAYER(s16_f16, int16_t, float,  CONV_NOOP);
DEFINE_SBUF_LAYER(s16_flt, int16_t, float,  CONV_S16_FLT);

DEFINE_SBUF_LAYER(flt_s16, float, int16_t,  CONV_FLT_S16);
DEFINE_SBUF_LAYER(flt_f16, float, float,    CONV_FLT_F16);
DEFINE_SBUF_LAYER(flt_flt, float, float,    CONV_NOOP);

DEFINE_SBUF_LAYER(f16_s16, float, int16_t,  CONV_F16_S16);
DEFINE_SBUF_LAYER(f16_f16, float, float,    CONV_NOOP);
DEFINE_SBUF_LAYER(f16_flt, float, float,    CONV_F16_FLT);

static sbuf_layer_t layer_matrix[SFMT_MAX][SFMT_MAX] = {
    { NULL, NULL, NULL, NULL },
    { NULL, sbuf_layer_s16_s16, sbuf_layer_s16_f16, sbuf_layer_s16_flt },
    { NULL, sbuf_layer_f16_s16, sbuf_layer_f16_f16, sbuf_layer_f16_flt },
    { NULL, sbuf_layer_flt_s16, sbuf_layer_flt_f16, sbuf_layer_flt_flt },
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
        VGM_LOG("SBUF: undefined layer function sfmt %i to %i\n", ssrc->fmt, sdst->fmt);
        return;
    }

    sbuf_layer_src_dst(ssrc->buf, sdst->buf, src_pos, dst_pos, src_copy, dst_max, ssrc->channels, sdst->channels);
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
        case SFMT_F16: {
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
