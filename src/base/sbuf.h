#ifndef _SBUF_H_
#define _SBUF_H_

#include "../streamtypes.h"

#if 0
/* interleaved: buffer for all channels = [ch*s] = (ch1 ch2 ch1 ch2 ch1 ch2 ch1 ch2 ...) */
/* planar: buffer per channel = [ch][s] = (c1 c1 c1 c1 ...)  (c2 c2 c2 c2 ...) */
typedef enum {
    SFMT_NONE,
    SFMT_S16,
    SFMT_F32,
  //SFMT_S24,
  //SFMT_S32,
  //SFMT_S16P,
  //SFMT_F32P,
} sfmt_t;


typedef struct {
    void* buf;          /* current sample buffer */
    int samples;        /* max samples */
    int channels;       /* interleaved step or planar buffers */
    sfmt_t fmt;         /* buffer type */
    //int filled;       /* samples in buffer */
    //int planar;
} sbuf_t;

void sbuf_init16(sbuf_t* sbuf, int16_t* buf, int samples, int channels);

void sbuf_clamp(sbuf_t* sbuf, int samples);

/* skips N samples from current sbuf */
void sbuf_consume(sbuf_t* sbuf, int samples);
#endif

/* it's probably slightly faster to make those inline'd, but aren't called that often to matter (given big enough total samples) */

// TODO decide if using float 1.0 style or 32767 style (fuzzy PCM changes when doing that)
void sbuf_copy_s16_to_f32(float* buf_f32, int16_t* buf_s16, int samples, int channels);

void sbuf_copy_f32_to_s16(int16_t* buf_s16, float* buf_f32, int samples, int channels);

void sbuf_copy_samples(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled);

void sbuf_copy_layers(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled, int dst_ch_start);

void sbuf_silence(sample_t* dst, int samples, int channels, int filled);

bool sbuf_realloc(sample_t** dst, int samples, int channels);

#endif
