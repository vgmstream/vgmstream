#ifndef _STRING_UTILS_
#define _STRING_UTILS_
#include <stddef.h>

/* ugly helpers since C strings functions are terrible */

/* copy from src to dst:
 * - always null-terminates, ignores NULL dst/src
 * - reads src up to max or dst_size (safe with untrusted src though not really that needed)
 * - dst and src must not overlap (uses memcpy)
 * - returns 0 is dst_max is 0 or dst NULL, copied size, or < if truncation occurs or src is null.
 */
int strcpy_v(char* dst, size_t dst_max, const char* src);

/* copy from src to dst
 * - same as v_strcpy but appends to dst
 */
int strcat_v(char* dst, size_t dst_max, const char* src);

#endif
