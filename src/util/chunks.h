#ifndef _UTIL_CHUNKS_H
#define _UTIL_CHUNKS_H

#include "../streamfile.h"

typedef struct {
    uint32_t type;      /* chunk id/fourcc */
    uint32_t size;      /* chunk size */
    uint32_t offset;    /* chunk offset (after type/size) */
    uint32_t current;   /* start position, or next chunk after size (set to -1 to break) */
    uint32_t max;       /* max offset, or filesize if not set */

    int le_type;        /* read type as LE instead of more common BE */
    int be_size;        /* read type as BE instead of more common LE */
    int full_size;      /* chunk size includes type+size */
    int alignment;      /* chunks with odd size need to be aligned to even, per RIFF spec */
} chunk_t;

/* reads from current offset and updates chunk_t */
int next_chunk(chunk_t* chunk, STREAMFILE* sf);

#if 0
enum { 
    CHUNK_RIFF = 0x52494646, /* "RIFF" */
    CHUNK_LIST = 0x4C495354, /* "LIST" */
};
#endif


/* chunk-style file helpers (the above is more performant, this is mainly for quick checks) */
int find_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t* p_chunk_offset, size_t* p_chunk_size, int big_endian_size, int zero_size_end);
/* find a RIFF-style chunk (with chunk_size not including id and size) */
int find_chunk_riff_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size);
int find_chunk_riff_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size);
/* same with chunk ids in variable endianess (so instead of "fmt " has " tmf" */
int find_chunk_riff_ve(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t* p_chunk_offset, size_t* p_chunk_size, int big_endian);


#endif
