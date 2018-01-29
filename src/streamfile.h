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
#if defined(ANDROID)
#include <unistd.h>
#endif

#if (defined(__MSVCRT__) || defined(_MSC_VER)) && !defined(ANDROID)
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


    /* Substream selection for files with subsongs. Manually used in metas if supported.
     * Not ideal here, but it's the simplest way to pass to all init_vgmstream_x functions. */
    int stream_index; /* 0=default/auto (first), 1=first, N=Nth */

} STREAMFILE;

/* create a STREAMFILE from path */
STREAMFILE * open_stdio_streamfile(const char * filename);

/* create a STREAMFILE from pre-opened file path */
STREAMFILE * open_stdio_streamfile_by_file(FILE * file, const char * filename);

/* A STREAMFILE that doesn't close the underlying stream.
 * Calls to open won't wrap the new SF (assumes it needs to be closed).
 * Can be used in metas to test custom IO without closing the external SF. */
STREAMFILE *open_wrap_streamfile(STREAMFILE *streamfile);

/* A STREAMFILE that clamps IO to a section of a larger stream.
 * Can be used with subfiles inside a bigger file, so it looks standard to a meta. */
STREAMFILE *open_clamp_streamfile(STREAMFILE *streamfile, off_t start, size_t size);

/* A STREAMFILE with custom IO, that clamps IO to a section of a larger stream.
 * Can be used with subfiles inside a bigger file, so it looks standard to a meta. */
STREAMFILE *open_io_streamfile(STREAMFILE *streamfile, void* data, size_t data_size, void* read_callback);//void* size_callback, void* seek_callback);

/* A STREAMFILE that reports a fake name, but still re-opens itself properly.
 * Can be used to trick a meta's extension check (to call from another, with a modified SF).
 * When fakename isn't supplied it's read from the streamfile, and the extension swapped with fakeext.
 * If the fakename is an existing file, open won't work on it as it'll reopen the fake-named streamfile. */
STREAMFILE *open_fakename_streamfile(STREAMFILE *streamfile, char * fakename, char * fakeext);

/* A streamfile formed from multiple streamfiles, their data joined during reads.
 * Can be used when data is segmented in multiple separate files.
 * The first streamfile is used to get names, stream index and so on. */
STREAMFILE *open_multifile_streamfile(STREAMFILE **streamfiles, size_t streamfiles_size);


/* close a file, destroy the STREAMFILE object */
static inline void close_streamfile(STREAMFILE * streamfile) {
    if (streamfile==NULL) return;
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

size_t get_streamfile_text_line(int dst_length, char * dst, off_t offset, STREAMFILE * streamfile, int *line_done_ptr);

STREAMFILE * open_stream_ext(STREAMFILE *streamFile, const char * ext);
STREAMFILE * open_stream_name(STREAMFILE *streamFile, const char * ext);

int read_string(char * buf, size_t bufsize, off_t offset, STREAMFILE *streamFile);

size_t read_key_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile);
int read_pos_file(uint8_t * buf, size_t bufsize, STREAMFILE *streamFile);

int check_extensions(STREAMFILE *streamFile, const char * cmp_exts);

int find_chunk_be(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size);
int find_chunk_le(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size);
int find_chunk(STREAMFILE *streamFile, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int size_big_endian, int zero_size_end);

int get_streamfile_name(STREAMFILE *streamFile, char * buffer, size_t size);
int get_streamfile_path(STREAMFILE *streamFile, char * buffer, size_t size);
int get_streamfile_ext(STREAMFILE *streamFile, char * filename, size_t size);
#endif
