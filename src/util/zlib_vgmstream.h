#ifndef _ZLIB_VGMSTREAM_H_
#define _ZLIB_VGMSTREAM_H_

/* ************************************************************************* */
/* use miniz (API-compatible) to avoid adding external zlib just for this codec
 * - https://github.com/richgel999/miniz
 *
 * define vgmstream's config for miniz, to avoid some warning and portability
 * issues, added here since we aren't passing external flags to miniz.c
 */

/* remove .zip handling stuff */
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS

/* force non-aligned reads to improve compiler warnings (slower tho) */
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
/* ************************************************************************* */

#include "miniz.h"

#endif
