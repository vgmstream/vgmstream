#ifndef _UTIL_ENDIAN_H
#define _UTIL_ENDIAN_H

#include "../streamfile.h"

typedef uint32_t (*read_u32_t)(off_t, STREAMFILE*);
typedef  int32_t (*read_s32_t)(off_t, STREAMFILE*);
typedef uint16_t (*read_u16_t)(off_t, STREAMFILE*);
typedef  int16_t (*read_s16_t)(off_t, STREAMFILE*);
typedef float (*read_f32_t)(off_t, STREAMFILE*);

typedef  int16_t (*get_s16_t)(const uint8_t*);

//todo move here
#define guess_endian32 guess_endianness32bit
#define guess_endian16 guess_endianness16bit

#endif
