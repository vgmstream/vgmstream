#ifndef _SBUF_H_
#define _SBUF_H_

#include "../streamtypes.h"

/* All types are interleaved (buffer for all channels = [ch*s] = ch1 ch2 ch1 ch2 ch1 ch2 ...)
 * rather than planar (buffer per channel = [ch][s] = c1 c1 c1 c1 ...  c2 c2 c2 c2 ...) */
typedef enum {
    SFMT_NONE,
    SFMT_S16,           /* standard PCM16 */
  //SFMT_S24,
  //SFMT_S32,
    SFMT_F32,           /* pcm-like float (+-32768), for internal use (simpler pcm > f32 plus some decoders use this) */
    SFMT_FLT,           /* standard float (+-1.0), for external players */
} sfmt_t;


/* simple buffer info to pass around, for internal mixing
 * meant to held existing sound buffer pointers rather than alloc'ing directly (some ops will swap/move its internals) */
typedef struct {
    void* buf;          /* current sample buffer */
    sfmt_t fmt;         /* buffer type */
    int channels;       /* interleaved step or planar buffers */
    int samples;        /* max samples */
    int filled;         /* samples in buffer */
} sbuf_t;

/* it's probably slightly faster to make some function inline'd, but aren't called that often to matter (given big enough total samples) */

void sbuf_init(sbuf_t* sbuf, sfmt_t format, void* buf, int samples, int channels);
void sbuf_init_s16(sbuf_t* sbuf, int16_t* buf, int samples, int channels);
void sbuf_init_f32(sbuf_t* sbuf, float* buf, int samples, int channels);

int sfmt_get_sample_size(sfmt_t fmt);

void* sbuf_get_filled_buf(sbuf_t* sbuf);

/* move buf by samples amount to simplify some code (will lose base buf pointer) */
void sbuf_consume(sbuf_t* sbuf, int count);

/* helpers to copy between buffers; note they assume dst and src aren't the same buf */
void sbuf_copy_to_f32(float* dst, sbuf_t* sbuf);
void sbuf_copy_from_f32(sbuf_t* sbuf, float* src);
void sbuf_copy_segments(sbuf_t* sdst, sbuf_t* ssrc);
void sbuf_copy_layers(sbuf_t* sdst, sbuf_t* ssrc, int dst_ch_start, int expected);

void sbuf_silence_s16(sample_t* dst, int samples, int channels, int filled);
void sbuf_silence_rest(sbuf_t* sbuf);
void sbuf_silence_part(sbuf_t* sbuf, int from, int count);

void sbuf_fadeout(sbuf_t* sbuf, int start, int to_do, int fade_pos, int fade_duration);

#endif
