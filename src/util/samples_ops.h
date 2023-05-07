#ifndef _SAMPLES_OPS_H
#define _SAMPLES_OPS_H

#include "../streamtypes.h"

/* swap samples in machine endianness to little endian (useful to write .wav) */
void swap_samples_le(sample_t* buf, int count);

#endif
