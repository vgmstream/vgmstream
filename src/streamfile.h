/*
* streamfile.h - definitions for buffered file reading with STREAMFILE
*/
#ifndef _STREAMFILE_H
#define _STREAMFILE_H

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

//TODO cleanup
//NULL, allocs
#include <stdlib.h>
//FILE
#include <stdio.h>
//string functions in meta and so on
#include <string.h>
//off_t
#include <sys/types.h>
#include "streamtypes.h"
#include "util.h"


/* MSVC fixes (though mingw uses MSVCRT but not MSC_VER, maybe use AND?) */
#if defined(__MSVCRT__) || defined(_MSC_VER)
    #include <io.h>
#endif

/* 64-bit offset is needed for banks that hit +2.5GB (like .fsb or .ktsl2stbin).
 * Leave as typedef to toggle since it's theoretically slower when compiled as 32-bit.
 * ATM it's only used in choice places until more performance tests are done.
 * uint32_t could be an option but needs to test when/how neg offsets are used.
 *
 * On POSIX 32-bit off_t can become off64_t by passing -D_FILE_OFFSET_BITS=64,
 * but not on MSVC as it doesn't have proper POSIX support, so a custom type is needed.
 * fseeks/tells also need to be adjusted for 64-bit support.
 */
typedef int64_t offv_t; //off64_t
//typedef int64_t sizev_t; // size_t int64_t off64_t


/* Streamfiles normally use an internal buffer to increase performance, configurable
 * but usually of this size. Lower increases the number of freads/system calls (slower).
 * However some formats need to jump around causing more buffer trashing than usual,
 * higher may needlessly read data that may be going to be trashed.
 *
 * Value can be adjusted freely but 8k is a good enough compromise. */
#define STREAMFILE_DEFAULT_BUFFER_SIZE 0x8000

/* struct representing a file with callbacks. Code should use STREAMFILEs and not std C functions
 * to do file operations, as plugins may need to provide their own callbacks.
 * Reads from arbitrary offsets, meaning internally may need fseek equivalents during reads. */
typedef struct _STREAMFILE {
    /* read 'length' data at 'offset' to 'dst' */
    size_t (*read)(struct _STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length);

    /* get max offset */
    size_t (*get_size)(struct _STREAMFILE* sf);

    //todo: DO NOT USE, NOT RESET PROPERLY (remove?)
    offv_t (*get_offset)(struct _STREAMFILE* sf);

    /* copy current filename to name buf */
    void (*get_name)(struct _STREAMFILE* sf, char* name, size_t name_size);

    /* open another streamfile from filename */
    struct _STREAMFILE* (*open)(struct _STREAMFILE* sf, const char* const filename, size_t buf_size);

    /* free current STREAMFILE */
    void (*close)(struct _STREAMFILE* sf);

    /* Substream selection for formats with subsongs.
     * Not ideal here, but it was the simplest way to pass to all init_vgmstream_x functions. */
    int stream_index; /* 0=default/auto (first), 1=first, N=Nth */

} STREAMFILE;

/* All open_ fuctions should be safe to call with wrong/null parameters.
 * _f versions are the same but free the passed streamfile on failure and return NULL,
 * to ease chaining by avoiding realloc-style temp ptr verbosity */

/* Opens a standard STREAMFILE, opening from path.
 * Uses stdio (FILE) for operations, thus plugins may not want to use it. */
STREAMFILE* open_stdio_streamfile(const char* filename);

/* Opens a standard STREAMFILE from a pre-opened FILE. */
STREAMFILE* open_stdio_streamfile_by_file(FILE* file, const char* filename);

/* Opens a STREAMFILE that does buffered IO.
 * Can be used when the underlying IO may be slow (like when using custom IO).
 * Buffer size is optional. */
STREAMFILE* open_buffer_streamfile(STREAMFILE* sf, size_t buffer_size);
STREAMFILE* open_buffer_streamfile_f(STREAMFILE* sf, size_t buffer_size);

/* Opens a STREAMFILE that doesn't close the underlying streamfile.
 * Calls to open won't wrap the new SF (assumes it needs to be closed).
 * Can be used in metas to test custom IO without closing the external SF. */
STREAMFILE* open_wrap_streamfile(STREAMFILE* sf);
STREAMFILE* open_wrap_streamfile_f(STREAMFILE* sf);

/* Opens a STREAMFILE that clamps reads to a section of a larger streamfile.
 * Can be used with subfiles inside a bigger file (to fool metas, or to simplify custom IO). */
STREAMFILE* open_clamp_streamfile(STREAMFILE* sf, offv_t start, size_t size);
STREAMFILE* open_clamp_streamfile_f(STREAMFILE* sf, offv_t start, size_t size);

/* Opens a STREAMFILE that uses custom IO for streamfile reads.
 * Can be used to modify data on the fly (ex. decryption), or even transform it from a format to another. 
 * Data is an optional state struct of some size what will be malloc+copied on open. */
STREAMFILE* open_io_streamfile(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback);
STREAMFILE* open_io_streamfile_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback);
/* Same, but calls init on SF open and close on close, when malloc/free is needed.
 * Data struct may be used to hold malloc'd pointers and stuff. */
STREAMFILE* open_io_streamfile_ex(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback);
STREAMFILE* open_io_streamfile_ex_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback);

/* Opens a STREAMFILE that reports a fake name, but still re-opens itself properly.
 * Can be used to trick a meta's extension check (to call from another, with a modified SF).
 * When fakename isn't supplied it's read from the streamfile, and the extension swapped with fakeext.
 * If the fakename is an existing file, open won't work on it as it'll reopen the fake-named streamfile. */
STREAMFILE* open_fakename_streamfile(STREAMFILE* sf, const char* fakename, const char* fakeext);
STREAMFILE* open_fakename_streamfile_f(STREAMFILE* sf, const char* fakename, const char* fakeext);

/* Opens streamfile formed from multiple streamfiles, their data joined during reads.
 * Can be used when data is segmented in multiple separate files.
 * The first streamfile is used to get names, stream index and so on. */
STREAMFILE* open_multifile_streamfile(STREAMFILE** sfs, size_t sfs_size);
STREAMFILE* open_multifile_streamfile_f(STREAMFILE** sfs, size_t sfs_size);

/* Opens a STREAMFILE from a (path)+filename.
 * Just a wrapper, to avoid having to access the STREAMFILE's callbacks directly. */
STREAMFILE* open_streamfile(STREAMFILE* sf, const char* pathname);

/* Reopen a STREAMFILE with a different buffer size, for fine-tuned bigfile parsing.
 * Uses default buffer size when buffer_size is 0 */
STREAMFILE* reopen_streamfile(STREAMFILE* sf, size_t buffer_size);


/* close a file, destroy the STREAMFILE object */
static inline void close_streamfile(STREAMFILE* sf) {
    if (sf != NULL)
        sf->close(sf);
}

/* read from a file, returns number of bytes read */
static inline size_t read_streamfile(uint8_t* dst, offv_t offset, size_t length, STREAMFILE* sf) {
    return sf->read(sf, dst, offset, length);
}

/* return file size */
static inline size_t get_streamfile_size(STREAMFILE* sf) {
    return sf->get_size(sf);
}

/* debug util, mainly for custom IO testing (num = writes file N, -1 = printfs, -2 = only reads) */
void dump_streamfile(STREAMFILE* sf, int num);

#endif
