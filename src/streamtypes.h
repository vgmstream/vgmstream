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
#endif /* (_MSC_VER >= 1600) */

#ifndef inline /* (_MSC_VER < 1900)? */
#define inline _inline
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#if (_MSC_VER < 1900)
#define snprintf _snprintf
#endif /* (_MSC_VER < 1900) */

#else
#include <stdint.h>
#endif /* _MSC_VER */

typedef int16_t sample; //TODO: deprecated, remove
typedef int16_t sample_t;

#endif
