/*
 * streamtypes.h - widely used type definitions
 */


#ifndef _STREAMTYPES_H
#define _STREAMTYPES_H

#ifdef _MSC_VER
/* Common versions:
 * - 1500: VS2008
 * - 1600: VS2010
 * - 1700: VS2012
 * - 1800: VS2013
 * - 1900: VS2015
 * - 1920: VS2019 */

#if (_MSC_VER >= 1600)

#include <stdint.h>
#else
#include <pstdint.h>
#endif

#if (_MSC_VER < 1800) && !defined(__cplusplus)
#define inline __inline
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#if (_MSC_VER < 1900)
#define snprintf _snprintf
#endif

#else
#include <stdint.h>

#endif /* _MSC_VER */

typedef int16_t sample; //TODO: deprecated, remove
typedef int16_t sample_t;

#endif
