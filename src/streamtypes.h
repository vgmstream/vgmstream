/*
 * streamtypes.h - widely used type definitions
 */
#ifndef _STREAMTYPES_H
#define _STREAMTYPES_H

#include <stddef.h> //size_t
#include <stdbool.h> //bool

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
  // _snprintf is *not* C99 compliant: on truncation returns -1 and **does not null-terminate**
  //#define snprintf _snprintf

  // the polyfill below should work but hasn't been tested
  #if 0
  #include <stdarg.h>
  #include <stdio.h>

  static int snprintf_msvc(char *buffer, size_t size, const char *format, ...) {
      int n;
      va_list args;

      va_start(args, format);
      n = _vsnprintf(buffer, size, format, args);
      va_end(args);

      if (size > 0) {
          buffer[size - 1] = '\0';
      }

      /* C99 snprintf behavior returns required length on truncation */
      if (n < 0) {
          va_list args2;
          va_start(args2, format);
          n = _vscprintf(format, args2);
          va_end(args2);
      }

      return n;
  }

  #define snprintf snprintf_msvc
  #endif

  #endif
#else

#include <stdint.h>

#endif /* _MSC_VER */

typedef int16_t sample_t;

#endif
