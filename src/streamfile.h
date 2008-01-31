/*
 * streamfile.h - definitions for buffered file reading with STREAMFILE
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include "streamtypes.h"

#ifndef _STREAMFILE_H
#define _STREAMFILE_H

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

/* read from a file
 *
 * returns number of bytes read
 */
size_t read_streamfile(uint8_t * dest, off_t offset, size_t length, STREAMFILE * streamfile);

/* return file size */
size_t get_streamfile_size(STREAMFILE * streamfile);

/* Sometimes you just need an int, and we're doing the buffering.
 * Note, however, that if these fail to read they'll return -1,
 * so that should not be a valid value or there should be some backup. */
int16_t read_16bitLE(off_t offset, STREAMFILE * streamfile);
int16_t read_16bitBE(off_t offset, STREAMFILE * streamfile);
int32_t read_32bitLE(off_t offset, STREAMFILE * streamfile);
int32_t read_32bitBE(off_t offset, STREAMFILE * streamfile);
int8_t read_8bit(off_t offset, STREAMFILE * streamfile);

#endif
