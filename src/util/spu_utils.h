#ifndef _SPU_UTILS_
#define _SPU_UTILS_
#include <stdint.h>

int spu1_pitch_to_sample_rate(int pitch);
int spu2_pitch_to_sample_rate(int pitch);

int spu1_pitch_to_sample_rate_rounded(int pitch);
int spu2_pitch_to_sample_rate_rounded(int pitch);

int spu1_note_to_pitch(int16_t note, int16_t fine, uint8_t center_note, uint8_t center_fine);
int spu2_note_to_pitch(int16_t note, int16_t fine, uint8_t center_note, uint8_t center_fine);

int square_key_to_sample_rate(int32_t key, int base_rate);
 
#endif
