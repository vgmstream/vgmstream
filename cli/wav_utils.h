#ifndef _WAV_UTILS_H_
#define _WAV_UTILS_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool is_float;
    int sample_size;

    int32_t sample_count;
    int32_t sample_rate;
    int channels;

    bool write_smpl_chunk;
    int32_t loop_start;
    int32_t loop_end;
} wav_header_t;

/* make a RIFF header for .wav; returns final RIFF size or 0 if buffer is too small */
size_t wav_make_header(uint8_t* buf, size_t buf_size, wav_header_t* wav);

/* swap big endian samples to little endian. Does nothing if machine is already LE.
 * Used when writting .WAV files, where samples in memory/buf may be BE while RIFF
 * is expected to have LE samples. */
void wav_swap_samples_le(void* samples, int samples_len, int sample_size);

#endif
