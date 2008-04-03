/*
 * streamfile.h - definitions for buffered file reading with STREAMFILE
 */

#ifdef _MSC_VER
	#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "streamtypes.h"
#include "util.h"

#ifndef _STREAMFILE_H
#define _STREAMFILE_H

#if defined(__MSVCRT__) || defined(_MSC_VER)
#define fseeko fseek
#define ftello ftell
#endif

#define STREAMFILE_DEFAULT_BUFFER_SIZE 0x400

typedef struct {
    FILE * infile;
    off_t offset;
    size_t validsize;
    uint8_t * buffer;
    size_t buffersize;
} STREAMFILE;

/* open file with a default buffer size, create a STREAMFILE object
 *
 * Returns pointer to new STREAMFILE or NULL if open failed
 */
STREAMFILE * open_streamfile(const char * const filename);
/* open file with a set buffer size, create a STREAMFILE object
 *
 * Returns pointer to new STREAMFILE or NULL if open failed
 */
STREAMFILE * open_streamfile_buffer(const char * const filename, size_t buffersize);

/* close a file, destroy the STREAMFILE object */
void close_streamfile(STREAMFILE * streamfile);

/* */
size_t read_the_rest(uint8_t * dest, off_t offset, size_t length, STREAMFILE * streamfile);

/* read from a file
 *
 * returns number of bytes read
 */
static inline size_t read_streamfile(uint8_t * dest, off_t offset, size_t length, STREAMFILE * streamfile) {
    if (!streamfile || !dest || length<=0) return 0;

    /* if entire request is within the buffer */
    if (offset >= streamfile->offset && offset+length <= streamfile->offset+streamfile->validsize) {
        memcpy(dest,streamfile->buffer+(offset-streamfile->offset),length);
        return length;
    }

    return read_the_rest(dest,offset,length,streamfile);
}

/* return file size */
size_t get_streamfile_size(STREAMFILE * streamfile);

/* Sometimes you just need an int, and we're doing the buffering.
 * Note, however, that if these fail to read they'll return -1,
 * so that should not be a valid value or there should be some backup. */
static inline int16_t read_16bitLE(off_t offset, STREAMFILE * streamfile) {
        uint8_t buf[2];

            if (read_streamfile(buf,offset,2,streamfile)!=2) return -1;
                return get_16bitLE(buf);
}
static inline int16_t read_16bitBE(off_t offset, STREAMFILE * streamfile) {
        uint8_t buf[2];

            if (read_streamfile(buf,offset,2,streamfile)!=2) return -1;
                return get_16bitBE(buf);
}
static inline int32_t read_32bitLE(off_t offset, STREAMFILE * streamfile) {
        uint8_t buf[4];

            if (read_streamfile(buf,offset,4,streamfile)!=4) return -1;
                return get_32bitLE(buf);
}
static inline int32_t read_32bitBE(off_t offset, STREAMFILE * streamfile) {
        uint8_t buf[4];

            if (read_streamfile(buf,offset,4,streamfile)!=4) return -1;
                return get_32bitBE(buf);
}

static inline int8_t read_8bit(off_t offset, STREAMFILE * streamfile) {
        uint8_t buf[1];

            if (read_streamfile(buf,offset,1,streamfile)!=1) return -1;
                return buf[0];
}

#endif
