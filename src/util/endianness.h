#ifndef _UTIL_ENDIAN_H
#define _UTIL_ENDIAN_H

#include "../streamfile.h"

typedef uint32_t (*read_u32_t)(off_t, STREAMFILE*);
typedef  int32_t (*read_s32_t)(off_t, STREAMFILE*);
typedef uint16_t (*read_u16_t)(off_t, STREAMFILE*);

#endif
