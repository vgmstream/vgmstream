#include <stdlib.h>
#include <string.h>
#include "../util.h"
#include "sbuf.h"

#if 0
/* skips N samples from current sbuf */
void sbuf_init16(sbuf_t* sbuf, int16_t* buf, int samples, int channels) {
    memset(sbuf, 0, sizeof(sbuf_t));
    sbuf->buf = buf;
    sbuf->samples = samples;
    sbuf->channels = channels;
    sbuf->fmt = SFMT_S16;
}
#endif


// TODO decide if using float 1.0 style or 32767 style (fuzzy PCM changes when doing that)
void sbuf_copy_s16_to_f32(float* buf_f32, int16_t* buf_s16, int samples, int channels) {
    for (int s = 0; s < samples * channels; s++) {
        buf_f32[s] = (float)buf_s16[s]; // / 32767.0f
    }
}

void sbuf_copy_f32_to_s16(int16_t* buf_s16, float* buf_f32, int samples, int channels) {
    /* when casting float to int, value is simply truncated:
     * - (int)1.7 = 1, (int)-1.7 = -1
     * alts for more accurate rounding could be:
     * - (int)floor(f)
     * - (int)(f < 0 ? f - 0.5f : f + 0.5f)
     * - (((int) (f1 + 32768.5)) - 32768)
     * - etc
     * but since +-1 isn't really audible we'll just cast, as it's the fastest
     */
    for (int s = 0; s < samples * channels; s++) {
        buf_s16[s] = clamp16( buf_f32[s]); // * 32767.0f
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

void sbuf_silence(sample_t* dst, int samples, int channels, int filled) {
    memset(dst + filled * channels, 0, (samples - filled) * channels * sizeof(sample_t));   
}

bool sbuf_realloc(sample_t** dst, int samples, int channels) {
    sample_t* outbuf_re = realloc(*dst, samples * channels * sizeof(sample_t));
    if (!outbuf_re) return false;

    *dst = outbuf_re;
    return true;
}
