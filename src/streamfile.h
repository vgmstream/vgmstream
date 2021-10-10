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

/* Opens a STREAMFILE from a base pathname + new extension
 * Can be used to get companion headers. */
STREAMFILE* open_streamfile_by_ext(STREAMFILE* sf, const char* ext);

/* Opens a STREAMFILE from a base path + new filename.
 * Can be used to get companion files. Relative paths like
 * './filename', '../filename', 'dir/filename' also work. */
STREAMFILE* open_streamfile_by_filename(STREAMFILE* sf, const char* filename);

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


/* Sometimes you just need an int, and we're doing the buffering.
* Note, however, that if these fail to read they'll return -1,
* so that should not be a valid value or there should be some backup. */
static inline int16_t read_16bitLE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[2];

    if (read_streamfile(buf,offset,2,sf)!=2) return -1;
    return get_16bitLE(buf);
}
static inline int16_t read_16bitBE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[2];

    if (read_streamfile(buf,offset,2,sf)!=2) return -1;
    return get_16bitBE(buf);
}
static inline int32_t read_32bitLE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf,offset,4,sf)!=4) return -1;
    return get_32bitLE(buf);
}
static inline int32_t read_32bitBE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf,offset,4,sf)!=4) return -1;
    return get_32bitBE(buf);
}
static inline int64_t read_64bitLE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,sf)!=8) return -1;
    return get_64bitLE(buf);
}
static inline int64_t read_64bitBE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,sf)!=8) return -1;
    return get_64bitBE(buf);
}
static inline int8_t read_8bit(off_t offset, STREAMFILE* sf) {
    uint8_t buf[1];

    if (read_streamfile(buf,offset,1,sf)!=1) return -1;
    return buf[0];
}

/* alias of the above */
static inline int8_t   read_s8   (off_t offset, STREAMFILE* sf) { return           read_8bit(offset, sf); }
static inline uint8_t  read_u8   (off_t offset, STREAMFILE* sf) { return (uint8_t) read_8bit(offset, sf); }
static inline int16_t  read_s16le(off_t offset, STREAMFILE* sf) { return           read_16bitLE(offset, sf); }
static inline uint16_t read_u16le(off_t offset, STREAMFILE* sf) { return (uint16_t)read_16bitLE(offset, sf); }
static inline int16_t  read_s16be(off_t offset, STREAMFILE* sf) { return           read_16bitBE(offset, sf); }
static inline uint16_t read_u16be(off_t offset, STREAMFILE* sf) { return (uint16_t)read_16bitBE(offset, sf); }
static inline int32_t  read_s32le(off_t offset, STREAMFILE* sf) { return           read_32bitLE(offset, sf); }
static inline uint32_t read_u32le(off_t offset, STREAMFILE* sf) { return (uint32_t)read_32bitLE(offset, sf); }
static inline int32_t  read_s32be(off_t offset, STREAMFILE* sf) { return           read_32bitBE(offset, sf); }
static inline uint32_t read_u32be(off_t offset, STREAMFILE* sf) { return (uint32_t)read_32bitBE(offset, sf); }
static inline int64_t  read_s64be(off_t offset, STREAMFILE* sf) { return           read_64bitBE(offset, sf); }
static inline uint64_t read_u64be(off_t offset, STREAMFILE* sf) { return (uint64_t)read_64bitBE(offset, sf); }
static inline int64_t  read_s64le(off_t offset, STREAMFILE* sf) { return           read_64bitLE(offset, sf); }
static inline uint64_t read_u64le(off_t offset, STREAMFILE* sf) { return (uint64_t)read_64bitLE(offset, sf); }

static inline float read_f32be(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_f32be(buf);
}
static inline float    read_f32le(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_f32le(buf);
}

#if 0
// on GCC, this reader will be correctly optimized out (as long as it's static/inline), would be same as declaring:
// uintXX_t (*read_uXX)(off_t,uint8_t*) = be ? get_uXXbe : get_uXXle;
// only for the functions actually used in code, and inlined if possible (like big_endian param being a constant).
// on MSVC seems all read_X in sf_reader are compiled and included in the translation unit, plus ignores constants
// so may result on bloatness?
// (from godbolt tests, test more real cases)

/* collection of callbacks for quick access */
typedef struct sf_reader {
    int32_t (*read_s32)(off_t,STREAMFILE*); //maybe r.s32
    float (*read_f32)(off_t,STREAMFILE*);
    /* ... */
} sf_reader;

static inline void sf_reader_init(sf_reader* r, int big_endian) {
    memset(r, 0, sizeof(sf_reader));
    if (big_endian) {
        r->read_s32 = read_s32be;
        r->read_f32 = read_f32be;
    }
    else {
        r->read_s32 = read_s32le;
        r->read_f32 = read_f32le;
    }
}

/* sf_reader r;
 * ...
 * sf_reader_init(&r, big_endian);
 * val = r.read_s32; //maybe r.s32?
 */
#endif
#if 0  //todo improve + test + simplify code (maybe not inline?)
static inline int read_s4h(off_t offset, STREAMFILE* sf) {
    uint8_t byte = read_u8(offset, streamfile);
    return get_nibble_signed(byte, 1);
}
static inline int read_u4h(off_t offset, STREAMFILE* sf) {
    uint8_t byte = read_u8(offset, streamfile);
    return (byte >> 4) & 0x0f;
}
static inline int read_s4l(off_t offset, STREAMFILE* sf) {
    ...
}
static inline int read_u4l(off_t offset, STREAMFILE* sf) {
    ...
}
static inline int max_s32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int min_s32(int32_t a, int32_t b) { return a < b ? a : b; }
//align32, align16, clamp16, etc
#endif

/* fastest to compare would be read_u32x == (uint32), but should be pre-optimized (see get_id32x) */
static inline /*const*/ int is_id32be(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u32be(offset, sf) == get_id32be(s);
}

static inline /*const*/ int is_id32le(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u32le(offset, sf) == get_id32be(s);
}

static inline /*const*/ int is_id64be(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u64be(offset, sf) == get_id64be(s);
}


//TODO: maybe move to streamfile.c
/* guess byte endianness from a given value, return true if big endian and false if little endian */
static inline int guess_endianness16bit(off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x02];
    if (read_streamfile(buf, offset, 0x02, sf) != 0x02) return -1; /* ? */
    return get_u16le(buf) > get_u16be(buf) ? 1 : 0;
}
static inline int guess_endianness32bit(off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x04];
    if (read_streamfile(buf, offset, 0x04, sf) != 0x04) return -1; /* ? */
    return get_u32le(buf) > get_u32be(buf) ? 1 : 0;
}

static inline size_t align_size_to_block(size_t value, size_t block_align) {
    size_t extra_size = value % block_align;
    if (extra_size == 0) return value;
    return (value + block_align - extra_size);
}

/* various STREAMFILE helpers functions */

/* Read into dst a line delimited by CRLF (Windows) / LF (Unux) / CR (Mac) / EOF, null-terminated
 * and without line feeds. Returns bytes read (including CR/LF), *not* the same as string length.
 * p_line_ok is set to 1 if the complete line was read; pass NULL to ignore. */
size_t read_line(char* buf, int buf_size, off_t offset, STREAMFILE* sf, int* p_line_ok);

/* skip BOM if needed */
size_t read_bom(STREAMFILE* sf);

/* reads a c-string (ANSI only), up to bufsize or NULL, returning size. buf is optional (works as get_string_size). */
size_t read_string(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf);
/* reads a UTF16 string... but actually only as ANSI (discards the upper byte) */
size_t read_string_utf16(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf, int big_endian);
size_t read_string_utf16le(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf);
size_t read_string_utf16be(char* buf, size_t buf_size, off_t offset, STREAMFILE* sf);

/* Opens a file containing decryption keys and copies to buffer.
 * Tries "(name.ext)key" (per song), "(.ext)key" (per folder) keynames.
 * returns size of key if found and copied */
size_t read_key_file(uint8_t* buf, size_t buf_size, STREAMFILE* sf);

/* Opens .txtm file containing file:companion file(-s) mappings and tries to see if there's a match
 * then loads the associated companion file if one is found */
STREAMFILE* read_filemap_file(STREAMFILE *sf, int file_num);
STREAMFILE* read_filemap_file_pos(STREAMFILE *sf, int file_num, int* p_pos);


/* hack to allow relative paths in various OSs */
void fix_dir_separators(char* filename);

/* Checks if the stream filename is one of the extensions (comma-separated, ex. "adx" or "adx,aix").
 * Empty is ok to accept files without extension ("", "adx,,aix"). Returns 0 on failure */
int check_extensions(STREAMFILE* sf, const char* cmp_exts);

/* chunk-style file helpers */
int find_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size, int big_endian_size, int zero_size_end);
/* find a RIFF-style chunk (with chunk_size not including id and size) */
int find_chunk_riff_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk_riff_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size);
/* same with chunk ids in variable endianess (so instead of "fmt " has " tmf" */
int find_chunk_riff_ve(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size, int big_endian);

/* filename helpers */
void get_streamfile_name(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_filename(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_basename(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_path(STREAMFILE* sf, char* buf, size_t size);
void get_streamfile_ext(STREAMFILE* sf, char* buf, size_t size);

void dump_streamfile(STREAMFILE* sf, int num);
#endif
