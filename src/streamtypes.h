/*
 * streamtypes.h - widely used type definitions
 */


#ifndef _STREAMTYPES_H
#define _STREAMTYPES_H

#ifdef _MSC_VER
#if (_MSC_VER >= 1600)
#include <stdint.h>
#else
#include <pstdint.h>
#endif
#define inline _inline
#define strcasecmp _stricmp
#if (_MSC_VER < 1900)
#define snprintf _snprintf
#endif
#else
#include <stdint.h>
#endif

typedef int16_t sample;

#endif
