#ifndef _SBUF_H
#define _SBUF_H

#include "../streamtypes.h"

// TODO decide if using float 1.0 style or 32767 style (fuzzy PCM changes when doing that)
static inline void sbuf_copy_s16_to_f32(float* buf_f32, int16_t* buf_s16, int samples, int channels) {
    for (int s = 0; s < samples * channels; s++) {
        buf_f32[s] = (float)buf_s16[s]; // / 32767.0f
    }
}

static inline void sbuf_copy_f32_to_s16(int16_t* buf_s16, float* buf_f32, int samples, int channels) {
    /* when casting float to int, value is simply truncated:
     * - (int)1.7 = 1, (int)-1.7 = -1
     * alts for more accurate rounding could be:
     * - (int)floor(f)
     * - (int)(f < 0 ? f - 0.5f : f + 0.5f)
     * - (((int) (f1 + 32768.5)) - 32768)
     * - etc
     * but since +-1 isn't really audible we'll just cast as it's the fastest
     */
    for (int s = 0; s < samples * channels; s++) {
        buf_s16[s] = clamp16( buf_f32[s]); // * 32767.0f
    }
}

#endif
