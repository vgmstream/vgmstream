/*
* streamfile.h - definitions for buffered file reading with STREAMFILE
*/

#ifndef _STREAMFILE_H
#define _STREAMFILE_H

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "streamtypes.h"
#include "util.h"

#if defined(__MSVCRT__) || defined(_MSC_VER)
#include <io.h>
#define fseeko fseek
#define ftello ftell
#define dup _dup
#ifdef fileno
#undef fileno
#endif
#define fileno _fileno
#define fdopen _fdopen
#endif

#if defined(XBMC)
#define fseeko fseek
#endif

#define STREAMFILE_DEFAULT_BUFFER_SIZE 0x8000

#ifndef DIR_SEPARATOR
#if defined (_WIN32) || defined (WIN32)
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif
#endif

/* struct representing a file with callbacks. Code should use STREAMFILEs and not std C functions
 * to do file operations, as plugins may need to provide their own callbacks. */
typedef struct _STREAMFILE {
    size_t (*read)(struct _STREAMFILE *,uint8_t * dest, off_t offset, size_t length);
    size_t (*get_size)(struct _STREAMFILE *);
    off_t (*get_offset)(struct _STREAMFILE *);    
    /* for dual-file support */
    void (*get_name)(struct _STREAMFILE *,char *name,size_t length);
    /* for when the "name" is encoded specially, this is the actual user visible name */
    void (*get_realname)(struct _STREAMFILE *,char *name,size_t length);
    struct _STREAMFILE * (*open)(struct _STREAMFILE *,const char * const filename,size_t buffersize);
    void (*close)(struct _STREAMFILE *);

#ifdef PROFILE_STREAMFILE
    size_t (*get_bytes_read)(struct _STREAMFILE *);
    int (*get_error_count)(struct _STREAMFILE *);
#endif


    /* Substream selection for files with multiple streams. Manually used in metas if supported.
     * Not ideal here, but it's the simplest way to pass to all init_vgmstream_x functions. */
    int stream_index; /* 0=default/auto (first), 1=first, N=Nth */

} STREAMFILE;

/* create a STREAMFILE from path */
STREAMFILE * open_stdio_streamfile(const char * filename);

/* create a STREAMFILE from pre-opened file path */
STREAMFILE * open_stdio_streamfile_by_file(FILE * file, const char * filename);


/* close a file, destroy the STREAMFILE object */
static inline void close_streamfile(STREAMFILE * streamfile) {
    streamfile->close(streamfile);
}

/* read from a file, returns number of bytes read */
static inline size_t read_streamfile(uint8_t * dest, off_t offset, size_t length, STREAMFILE * streamfile) {
    return streamfile->read(streamfile,dest,offset,length);
}

/* return file size */
static inline size_t get_streamfile_size(STREAMFILE * streamfile) {
    return streamfile->get_size(streamfile);
}

#ifdef PROFILE_STREAMFILE
/* return how many bytes we read into buffers */
static inline size_t get_streamfile_bytes_read(STREAMFILE * streamfile) {
    if (streamfile->get_bytes_read)
        return streamfile->get_bytes_read(streamfile);
    else
        return 0;
}

/* return how many times we encountered a read error */
static inline int get_streamfile_error_count(STREAMFILE * streamfile) {
    if (streamfile->get_error_count)
        return streamfile->get_error_count(streamfile);
    else
        return 0;
}
#endif

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
static inline int64_t read_64bitLE(off_t offset, STREAMFILE * streamfile) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,streamfile)!=8) return -1;
    return get_64bitLE(buf);
}
static inline int64_t read_64bitBE(off_t offset, STREAMFILE * streamfile) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,streamfile)!=8) return -1;
    return get_64bitBE(buf);
}

static inline int8_t read_8bit(off_t offset, STREAMFILE * streamfile) {
    uint8_t buf[1];

    if (read_streamfile(buf,offset,1,streamfile)!=1) return -1;
    return buf[0];
}

/* various STREAMFILE helpers functions */

size_t get_streamfile_dos_line(int dst_length, char * dst, off_t offset, STREAMFILE * infile, int *line_done_ptr);

STREAMFILE * open_stream_ext(STREAMFILE *streamFile, const char * ext);

int read_string(char * buf, size_t bufsize, off_t offset, STREAMFILE *streamFile);

int read_key_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile);
int read_pos_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile);

int check_extensions(STREAMFILE *streamFile, const char * cmp_exts);

int find_chunk_be(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size);
int find_chunk_le(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size);
int find_chunk(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int size_big_endian, int zero_size_end);

int get_streamfile_name(STREAMFILE *streamFile, char * buffer, size_t size);
int get_streamfile_path(STREAMFILE *streamFile, char * buffer, size_t size);
int get_streamfile_ext(STREAMFILE *streamFile, char * filename, size_t size);
#endif
