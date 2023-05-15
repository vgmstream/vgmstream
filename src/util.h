/*
 * util.h - utility functions
 */
#ifndef _UTIL_H
#define _UTIL_H

#include "streamtypes.h"
#include "util/reader_get.h"
#include "util/reader_put.h"

/* very common functions, so static (inline) in .h as compiler can optimize to avoid some call overhead */

static inline int clamp16(int32_t val) {
    if (val > 32767) return 32767;
    else if (val < -32768) return -32768;
    else return val;
}


/* transforms a string to uint32 (for comparison), but if this is static + all goes well
 * compiler should pre-calculate and use uint32 directly */
static inline /*const*/ uint32_t get_id32be(const char* s) {
    return (uint32_t)((uint8_t)s[0] << 24) | ((uint8_t)s[1] << 16) | ((uint8_t)s[2] << 8) | ((uint8_t)s[3] << 0);
}

//static inline /*const*/ uint32_t get_id32le(const char* s) {
//    return (uint32_t)(s[0] << 0) | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);
//}

static inline /*const*/ uint64_t get_id64be(const char* s) {
    return (uint64_t)(
            ((uint64_t)s[0] << 56) |
            ((uint64_t)s[1] << 48) |
            ((uint64_t)s[2] << 40) |
            ((uint64_t)s[3] << 32) |
            ((uint64_t)s[4] << 24) |
            ((uint64_t)s[5] << 16) |
            ((uint64_t)s[6] << 8) |
            ((uint64_t)s[7] << 0)
    );
}


/* less common functions, no need to inline */

int round10(int val);

/* return a file's extension (a pointer to the first character of the
 * extension in the original filename or the ending null byte if no extension */
const char* filename_extension(const char* pathname);

void concatn(int length, char * dst, const char * src);

size_t align_size_to_block(size_t value, size_t block_align);

#endif
