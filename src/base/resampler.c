#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "resampler.h"

/*
 * resampling 101:
 * - given N samples + ratio, use a function to calculate "in between" samples: y(t) = new_sample
 *   ex. src at 1.0 ratio = y(0) y(1) y(2) ... but at 1.23 ratio = y(0) y(1/1.23) y(2/1.23) ...
 *   (put X data/samples and get N samples, not unlike a decoder)
 * - lower ratio = more samples, higher = less (44100 to 48000: 0.91875, 44100 pitch +10% faster: 1.10)
 * - functions may need prev (y[-N]) and next (y[+N]) samples to generate a current sample at new ratio.
 * - resampler types:
 *   - polynomial interpolators (math formula of N points)
 *   - convolution-based like sinc/fir filters (weighted curve of +-N taps + window filter)
 *   - tranformation-based/FFT (complex and not covered here)
 * - better resamplers are slower and have more 'delay' (samples needed to calculate next sample)
 *   but create less spectrum artifacts
 * - simpler ratios create better results: 24000 to 48000 = 0.5, while 44100 to 48000 = 0.91875
 * - since we have discrete stream buffers, resamplers need internal state to handle multiple separate bufs
 * - also need to output ("drain") when there are won't be more src bufs at EOF but still needs to output a few edge samples
 *
 * Some references:
 * - https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
 * - https://www.slack.net/~ant/
 * - https://github.com/ahigerd
 * - https://github.com/CyberBotX
 *
 */
/* TODO:
 * - allow bigger ring_bufs to copy more at once if possible
 * - instead of making dst as big as possible, use "feed" flags and mark consumed src
 * - allow chaging ratio on real time (would need to reset init ringbuf and sinc)
 */


#define SUBSAMPLE_ONE (1ULL << 32)
#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif
// arbitrary limits
#define MIN_CHANNELS 1
#define MAX_CHANNELS 64
#define MIN_RATIO (3000.0 / 192000.0)
#define MAX_RATIO (192000.0 / 3000.0)


struct resampler_ctx_t {
    resampler_cfg_t cfg;

    sbuf_t dst;             // output buf, lasts until next call

    int delay;              // number of prev samples needed (info for now)
    int points;             // number of samples needed to output next dst sample (including delay)

    int sinc_width;         // 'taps' in the sinc kernel (N samples)
    int sinc_resolution;    // fractional steps of the sinc (LUT density)
    int sinc_samples;       // max LUT index
    float* sinc_lut;        // precalculated sinc function coefs (distance)
    float* window_lut;      // precalculated window coefs

    int ring_filled;        // how many non-consumed samples are in ringbuf
    int ring_pos;           // current pos in ring_buf
    uint32_t ring_mask;     // ring_pos mask
    float* ring_buf;        // temp ring buffer of N points

  //double subsample;       // current step (since current resampled position falls between discrete samples)
    uint64_t subsample_fp;  // fixed point subsample, to prevent float drifting (probably not too noticeable though)
    uint64_t ratio_fp;      // fixed point ratio * 2^32

    float (*resample)(resampler_ctx_t* ctx, int ch); // get next sample based on current state
};

//*****************************************************************************

// calculate number of bits needed for ringbuf size's mask (power of 2)
static int ringbuf_bits(int points) {
    //return (int)log2(points) + 1; //TODO math.h issues

    if      (points <= 1)   return 1;
    else if (points <= 3)   return 2;
    else if (points <= 7)   return 3;
    else if (points <= 15)  return 4;
    else if (points <= 31)  return 5;
    else if (points <= 63)  return 6;
    else if (points <= 127) return 7;
    else if (points <= 255) return 8;
    else return 0;
}

static inline void ringbuf_reset(resampler_ctx_t* ctx, int points) {
    int mask_bits = ringbuf_bits(points);
    if (mask_bits <= 0) return;

    memset(ctx->ring_buf, 0, (1 << mask_bits) * ctx->cfg.channels * sizeof(float));

    ctx->ring_pos = 0;
    ctx->ring_filled = 0;
}

static inline bool ringbuf_init(resampler_ctx_t* ctx, int points) {

    int mask_bits = ringbuf_bits(points);
    if (mask_bits <= 0) return false;

    ctx->ring_mask = (1 << mask_bits) - 1; // note that mask can be larger than points

    // interpolators that negative samples will use 0 at the beginning of the file but seems correct/standard
    // (for example repeating the 1st sample seems to cause clicks sometimes)
    // must hold as many bits needed for the mask, even with less points
    ctx->ring_buf = malloc((1 << mask_bits) * ctx->cfg.channels * sizeof(float));
    if (!ctx->ring_buf) return false;

    ringbuf_reset(ctx, points);

    return true;
}

static inline void ringbuf_free(resampler_ctx_t* ctx) {
    free(ctx->ring_buf);
}

static inline float ringbuf_get(resampler_ctx_t* ctx, int index, int ch) {
    int channels = ctx->cfg.channels;

    int pos = (ctx->ring_pos + index) & ctx->ring_mask;
    return ctx->ring_buf[pos * channels + ch];
}

static inline void ringbuf_consume(resampler_ctx_t* ctx, int samples) {
    ctx->ring_pos = (ctx->ring_pos + samples) & ctx->ring_mask;
    ctx->ring_filled -= samples;
}

//*****************************************************************************

// subsample position in [0.0..1.0)
static double get_mu(resampler_ctx_t* ctx) {
    //return ctx->subsample;
    return (double)(ctx->subsample_fp & 0xFFFFFFFFULL) * (1.0 / 4294967296.0);

}

static float resample_linear(resampler_ctx_t* ctx, int ch) {
    double mu = get_mu(ctx);
    float y0 = ringbuf_get(ctx, 0, ch);
    float y1 = ringbuf_get(ctx, 1, ch);
    return y0 + (y1 - y0) * mu;
}

// 4-point, 3rd-order Hermite (x-form) AKA cubic Hermite or Catmull-Rom
static float resample_hermite4(resampler_ctx_t* ctx, int ch) {
    double mu = get_mu(ctx);
    float y0 = ringbuf_get(ctx, -1, ch);
    float y1 = ringbuf_get(ctx,  0, ch);
    float y2 = ringbuf_get(ctx,  1, ch);
    float y3 = ringbuf_get(ctx,  2, ch);

    float c0 = y1;
    float c1 = 1/2.0 * (y2 - y0);
    float c2 = y0 - (5/2.0 * y1) + (2.0 * y2) - (1/2.0 * y3);
    float c3 = (1/2.0 * (y3 - y0)) + (3/2.0 * (y1 - y2));
    return ((c3 * mu + c2) * mu + c1) * mu + c0;
}

// 4-point, 3rd-order Lagrange (x-form)
static float resample_lagrange4(resampler_ctx_t* ctx, int ch) {
    double mu = get_mu(ctx);
    float y0 = ringbuf_get(ctx, -1, ch);
    float y1 = ringbuf_get(ctx,  0, ch);
    float y2 = ringbuf_get(ctx,  1, ch);
    float y3 = ringbuf_get(ctx,  2, ch);

    float c0 = y1;
    float c1 = y2 - 1/3.0 * y0 - 1/2.0 * y1 - 1/6.0 * y3;
    float c2 = 1/2.0 * (y0 + y2) - y1;
    float c3 = 1/6.0 * (y3 - y0)  +  1/2.0 * (y1 - y2);
    return ((c3 * mu + c2) * mu + c1) * mu + c0;
}

// 6-point, 5th-order Lagrange (x-form)
static float resample_lagrange6(resampler_ctx_t* ctx, int ch) {
    double mu = get_mu(ctx);
    float y0 = ringbuf_get(ctx, -2, ch);
    float y1 = ringbuf_get(ctx, -1, ch);
    float y2 = ringbuf_get(ctx,  0, ch);
    float y3 = ringbuf_get(ctx,  1, ch);
    float y4 = ringbuf_get(ctx,  2, ch);
    float y5 = ringbuf_get(ctx,  3, ch);

    float c0 = y2;
    float c1 = 1/20.0 * y0 - 1/2.0 * y1 - 1/3.0 * y2 + y3 - 1/4.0 * y4 + 1/30.0 * y5;
    float c2 = 2/3.0 * (y1 + y3) - 5/4.0 * y2 - (1/24.0 * (y0 + y4));
    float c3 = 5/12.0 * y2 - 7/12.0 * y3 + 7/24.0 * y4 - 1/24.0 * (y0 + y1 + y5);
    float c4 = 1/4.0 * y2 - 1/6.0 * (y1 + y3)  +  (1/24.0 * (y0 + y4));
    float c5 = 1/120.0 * (y5 - y0)  +  1/24.0 * (y1 - y4) + 1/12.0 * (y3 - y2);

    return ((((c5 * mu + c4) * mu + c3) * mu + c2) * mu + c1) * mu + c0;
}

static float resample_sinc(resampler_ctx_t* ctx, int ch) {
    int width_half = ctx->sinc_width >> 1;
    double mu = get_mu(ctx) - 0.5; // recenter around middle of sinc

    double sum = 0.0;
    double kernel_sum = 0.0;
    for (int i = -width_half; i < width_half; i++) {
        double dt = fabs(i - mu); // distance from center
        int idx = dt * ctx->sinc_resolution; // map distance to LUT
        if (idx > ctx->sinc_samples)
            continue;

        double kernel = ctx->sinc_lut[idx] * ctx->window_lut[idx];
        kernel_sum += kernel;

        float y = ringbuf_get(ctx, i, ch);
        sum += y * kernel;
    }

    if (kernel_sum == 0.0)
        return 0.0f;
    return (sum / kernel_sum);
}

//*****************************************************************************

static bool itrp_init(resampler_ctx_t* ctx, int points, int delay) {

    ctx->delay = delay;
    ctx->points = points;

    bool ok = ringbuf_init(ctx, points);
    if (!ok) return false;

    return true;
}

static void sinc_free(resampler_ctx_t* ctx) {
    if (!ctx)
        return;
    free(ctx->sinc_lut);
    free(ctx->window_lut);
}

// compute LUTs for 0..half and window
static bool sinc_init(resampler_ctx_t* ctx, int width, int resolution) {
    // only even widths for symmetry
    if (width <= 0 || (width % 2) != 0)
        return false;

    int width_half = width >> 1; //symetric around center

    ctx->sinc_width      = width;
    ctx->sinc_resolution = resolution;
    ctx->sinc_samples    = ctx->sinc_resolution * width_half;

    ctx->sinc_lut   = malloc((ctx->sinc_samples + 1) * sizeof(float));
    ctx->window_lut = malloc((ctx->sinc_samples + 1) * sizeof(float));
    if (!ctx->sinc_lut || !ctx->window_lut) goto fail;

    // first entry is fixed, last entry is sinc_samples (+1)
    ctx->sinc_lut[0] = 1.0;
    ctx->window_lut[0] = 1.0;
    for (int i = 1; i <= ctx->sinc_samples; i++) {
        double x = (i / (double)ctx->sinc_resolution) * M_PI;
        ctx->sinc_lut[i] = sin(x) / x;

        double y = (i / (double)ctx->sinc_samples) * M_PI; //w-phase?
        ctx->window_lut[i] = 0.40897 + 0.5 * cos(y) + 0.09103 * cos(2.0 * y); // Nuttal 3-term window
    }

    // setup ringbuf
    if (!itrp_init(ctx, width, width / 2))
        goto fail;

    return true;

fail:
    free(ctx->sinc_lut);
    free(ctx->window_lut);
    ctx->sinc_lut   = NULL;
    ctx->window_lut = NULL;
    return false;
}

static void resampler_set_ratio(resampler_ctx_t* ctx, double ratio) {
    if (!ctx) return;

    if (ratio < MIN_RATIO)
        ratio = MIN_RATIO;
    if (ratio > MAX_RATIO)
        ratio = MAX_RATIO;

    ctx->cfg.ratio = ratio;
    ctx->ratio_fp = (uint64_t)(ratio * (double)(1ULL << 32));

    // implicitly will redo the dst buf on push so it would be fine to change
}


void* resampler_init(resampler_cfg_t* cfg) {
    bool ok = false;

    resampler_ctx_t* ctx = calloc(1, sizeof(resampler_ctx_t));
    if (!ctx) return NULL;

    if (cfg->channels < MIN_CHANNELS || cfg->channels > MAX_CHANNELS)
        goto fail;

    if (ctx->cfg.type == RESAMPLER_TYPE_DEFAULT)
        ctx->cfg.type = RESAMPLER_TYPE_LAGRANGE6;

    ctx->cfg = *cfg;
    resampler_set_ratio(ctx, cfg->ratio);

    switch (ctx->cfg.type) {
        case RESAMPLER_TYPE_LINEAR:
            ok = itrp_init(ctx, 2, 0);
            ctx->resample = resample_linear;
            break;
        case RESAMPLER_TYPE_HERMITE4:
            ctx->resample = resample_hermite4;
            ok = itrp_init(ctx, 4, 1);
            break;
        case RESAMPLER_TYPE_LAGRANGE4:
            ctx->resample = resample_lagrange4;
            ok = itrp_init(ctx, 4, 1);
            break;
        case RESAMPLER_TYPE_LAGRANGE6:
            ctx->resample = resample_lagrange6;
            ok = itrp_init(ctx, 4, 2);
            break;
        case RESAMPLER_TYPE_SINC:
            ctx->resample = resample_sinc;
            ok = sinc_init(ctx, 32, 1024); //64 taps is a bit costly
            break;
        default:
            goto fail;
    }

    if (!ok)
        goto fail;    

    return ctx;
fail:
    resampler_free(ctx);
    return NULL;
}

void resampler_free(resampler_ctx_t* ctx) {
    if (!ctx) return;

    sinc_free(ctx);
    ringbuf_free(ctx);
    free(ctx);
}

void resampler_reset(resampler_ctx_t* ctx) {
    if (!ctx) return;

    ringbuf_reset(ctx, ctx->points);

    //ctx->subsample = 0.0;
    ctx->subsample_fp = 0;

    // (dst is reset on push)
}

//*****************************************************************************

static bool reserve_dst(resampler_ctx_t* ctx, sbuf_t* sbuf) {

    if (ctx->cfg.channels != sbuf->channels)
        return false;

    // +1/2 extra should be enough for ceil cases, but reserve a bit more just in case
    int min_samples = sbuf->filled * (1.0f / ctx->cfg.ratio) + 256; 
    if (ctx->dst.samples >= min_samples)
        return true;

    void* dst_new = malloc(min_samples * ctx->cfg.channels * sizeof(float));
    if (!dst_new) return false;

    // keep old samples when ratio changes and need a bigger buf
    if (ctx->dst.buf) {
        memcpy(dst_new, ctx->dst.buf, ctx->dst.filled * ctx->dst.channels * sizeof(float));
        free(ctx->dst.buf);
    }
    else {
        ctx->dst.filled = 0;
        ctx->dst.channels = sbuf->channels;
    }

    ctx->dst.samples = min_samples;
    ctx->dst.buf = dst_new;
    ctx->dst.fmt = SFMT_FLT; //TODO: keep F16 if possible?
    ctx->dst.channels = ctx->cfg.channels;

    return true;
}

#if 0
static int16_t clamp_f32(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return (int16_t)sample;
}
#endif

// resample and produce 1 sample at dst
static void produce_sample(resampler_ctx_t* ctx, sbuf_t* dst) {
    int channels = ctx->cfg.channels;
    float* dst_buf = dst->buf;

    for (int ch = 0; ch < channels; ch++) {
        float out_s = ctx->resample(ctx, ch);
        dst_buf[dst->filled * channels + ch] = out_s; //clamp_f32(out_s);
    }
    dst->filled++;
}

// add new sample at ring_buf's edge
static void add_sample(resampler_ctx_t* ctx, sbuf_t* src, int src_pos) {
    int channels = ctx->cfg.channels;

    int put_pos = (ctx->ring_pos + ctx->ring_filled) & ctx->ring_mask;
    for (int ch = 0; ch < channels; ch++) {
        float in_s;

        // TODO: improve (doing this sample by sample is not ideal)
        switch (src->fmt) {
            case SFMT_S16: {
                int16_t* src_buf = src->buf;
                in_s = src_buf[src_pos * channels + ch] * (1.0f / 32767.0f);
                break;
            }
            case SFMT_F16: {
                float* src_buf = src->buf;
                in_s = src_buf[src_pos * channels + ch] * (1.0f / 32767.0f);
                break;
            }
            case SFMT_FLT: {
                float* src_buf = src->buf;
                in_s = src_buf[src_pos * channels + ch] * (1.0f / 32767.0f);
                break;
            }
            case SFMT_S24: {
                int32_t* src_buf = src->buf;
                in_s = src_buf[src_pos * channels + ch] * (1.0f / 8388607.0f);
                break;
            }
            case SFMT_S32: {
                int32_t* src_buf = src->buf;
                in_s = src_buf[src_pos * channels + ch] * (1.0f / 2147483647.0f);
                break;
            }
            case SFMT_NONE: // used when draining
            default: {
                in_s = 0.0;
                break;
            }
        }

        ctx->ring_buf[put_pos * channels + ch] = in_s;
    }
    ctx->ring_filled++;
}

// Consume samples from src and produce as many dst samples as possible.
// Uses a ringbuf to copy N samples needed to resample (up to ->points).
int resampler_push_samples(resampler_ctx_t* ctx, sbuf_t* src) {
    sbuf_t* dst = &ctx->dst;

    // resize dst buf if needed
    bool ok = reserve_dst(ctx, src);
    if (!ok) return RESAMPLER_RES_ERROR;

    int src_pos = 0;
    dst->filled = 0; // fills again on each push

    // main process
    while (dst->filled < dst->samples) {

        // refill ringbuf if needed and possible
        while (src_pos < src->filled && ctx->ring_filled < ctx->points)  {
            add_sample(ctx, src, src_pos++);
        }

        // move ringbuf window if last step is too big
        if (ctx->subsample_fp >= SUBSAMPLE_ONE) {  //if (ctx->subsample >= 1.0) {
            if (ctx->ring_filled <= 0) {
                if (src_pos < src->filled)
                    continue; // refill and keep stepping
                break; // no more data
            }

            ctx->subsample_fp -= SUBSAMPLE_ONE;  //ctx->subsample -= 1.0;
            ringbuf_consume(ctx, 1);
            continue; // keep stepping and filling
        }

        // break if we can't produce more samples
        if (ctx->ring_filled < ctx->points) {
            if (src_pos < src->filled)
                continue; //refill
            break;  //no more data
        }

        // generate 1 sample from current resample window
        produce_sample(ctx, dst);
        ctx->subsample_fp += ctx->ratio_fp; //ctx->subsample += ctx->cfg.ratio;
    }

    // all src should be consumed
    return RESAMPLER_RES_OK;
}


int resampler_get_samples(resampler_ctx_t* ctx, sbuf_t* dst) {

    *dst = ctx->dst;

    return RESAMPLER_RES_OK;
}

int resampler_drain_samples(resampler_ctx_t* ctx, sbuf_t* dst) {

    //TODO: some ratios seem to result in incorrect +-1
    // remove extra samples to fix off-by-one in some cases
    int target_filled = (int)((ctx->ring_filled / ctx->cfg.ratio));

    // 'fill' buf just enough to flush remaining samples
    int pad = ctx->points - (ctx->points - ctx->ring_filled);

    sbuf_t src = {
        .buf = NULL,
        .filled = pad,
        .channels = ctx->cfg.channels,
        .fmt = SFMT_NONE
    };

    resampler_push_samples(ctx, &src);
    resampler_get_samples(ctx, dst);

    if (dst->filled > target_filled)
        dst->filled = target_filled;

    return RESAMPLER_RES_OK;
}
