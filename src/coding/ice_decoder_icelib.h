#ifndef _ICELIB_H_
#define _ICELIB_H_
#include <stdint.h>

/* Decodes Inti Creates's "ICE" engine BIGRP sounds. */

#define ICESND_CODEC_RANGE     0x00
#define ICESND_CODEC_DATA      0x01
#define ICESND_CODEC_MIDI      0x02
#define ICESND_CODEC_DCT       0x03

#define ICESND_RESULT_OK       0
#define ICESND_ERROR_HEADER    -1
#define ICESND_ERROR_SETUP     -2
#define ICESND_ERROR_DECODE    -3

typedef struct icesnd_handle_t icesnd_handle_t;

#define ICESND_SEEK_SET    0
#define ICESND_SEEK_CUR    1
#define ICESND_SEEK_END    2

typedef struct {
    /* whole file in memory (for testing) */
    const uint8_t* filebuf;
    int filebuf_size;

    /* custom IO */
	void* arg;
    int (*read)(void* dst, int size, int n, void* arg);
	int (*seek)(void* arg, int offset, int whence);

} icesnd_callback_t;

/* Inits ICE lib with config.
 * Original code expects all data in memory, but this allows setting read callbacks
 * (making it feed-style was a bit complex due to how data is laid out) */
icesnd_handle_t* icesnd_init(int target_subsong, icesnd_callback_t* cb);

void icesnd_free(icesnd_handle_t* handle);

/* resets the decoder. If loop_starts and file loops and 
 * (format is not seekable but separated into intro+body blocks) */
void icesnd_reset(icesnd_handle_t* handle, int loop_start);

/* Decodes up to samples for N channels into sbuf (interleaved). Returns samples done, 
 * 0 if not possible (non-looped files past end) or negative on error.
 * May return less than wanted samples on block boundaries.
 * 
 * It's designed to decode an arbitrary number of samples, as data isn't divided into frames (original
 * player does sample_rate/60.0 at a time). Codec 0 is aligned to 100 samples and codec 3 to 16 though. */
int icesnd_decode(icesnd_handle_t* handle, int16_t* sbuf, int max_samples);

typedef struct {
    int total_subsongs;
    int codec;
    int sample_rate;
    int channels;
    int loop_start;
    int num_samples;
    int loop_flag;
} icesnd_info_t;

/* loads info */
int icesnd_info(icesnd_handle_t* handle, icesnd_info_t* info);

#endif
