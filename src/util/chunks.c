#include "chunks.h"
#include "reader_sf.h"


int next_chunk(chunk_t* chunk, STREAMFILE* sf) {
    uint32_t (*read_u32type)(off_t,STREAMFILE*) = !chunk->le_type ? read_u32be : read_u32le;
    uint32_t (*read_u32size)(off_t,STREAMFILE*) = chunk->be_size ? read_u32be : read_u32le;

    if (chunk->max == 0)
        chunk->max = get_streamfile_size(sf);

    if (chunk->current >= chunk->max)
        return 0;
    /* can be used to signal "stop" */
    if (chunk->current < 0)
        return 0;

    chunk->type = read_u32type(chunk->current + 0x00,sf);
    chunk->size = read_u32size(chunk->current + 0x04,sf);

    chunk->offset = chunk->current + 0x04 + 0x04;
    chunk->current += chunk->full_size ? chunk->size : 0x08 + chunk->size;
    //;VGM_LOG("CHUNK: %x, %x, %x\n", dc.offset, chunk->type, chunk->size);

    /* read past data */
    if (chunk->type == 0xFFFFFFFF || chunk->size == 0xFFFFFFFF)
        return 0;

    /* empty chunk with 0 size is ok, seen in some formats (XVAG uses it as end marker, Wwise in JUNK) */
    if (chunk->type == 0 /*|| chunk->size == 0*/)
        return 0;

    /* more chunks remain */
    return 1;
}


/* ************************************************************************* */

/**
 * Find a chunk starting from an offset, and save its offset/size (if not NULL), with offset after id/size.
 * Works for chunked headers in the form of "chunk_id chunk_size (data)"xN  (ex. RIFF).
 * The start_offset should be the first actual chunk (not "RIFF" or "WAVE" but "fmt ").
 * "full_chunk_size" signals chunk_size includes 4+4+data.
 *
 * returns 0 on failure
 */
static int find_chunk_internal(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian_type, int big_endian_size, int zero_size_end) {
    int32_t (*read_32bit_type)(off_t,STREAMFILE*) = big_endian_type ? read_32bitBE : read_32bitLE;
    int32_t (*read_32bit_size)(off_t,STREAMFILE*) = big_endian_size ? read_32bitBE : read_32bitLE;
    off_t offset, max_offset;
    size_t file_size = get_streamfile_size(sf);

    if (max_size == 0)
        max_size = file_size;

    offset = start_offset;
    max_offset = offset + max_size;
    if (max_offset > file_size)
        max_offset = file_size;


    /* read chunks */
    while (offset < max_offset) {
        uint32_t chunk_type = read_32bit_type(offset + 0x00,sf);
        uint32_t chunk_size = read_32bit_size(offset + 0x04,sf);

        if (chunk_type == 0xFFFFFFFF || chunk_size == 0xFFFFFFFF)
            return 0;

        if (chunk_type == chunk_id) {
            if (out_chunk_offset) *out_chunk_offset = offset + 0x08;
            if (out_chunk_size) *out_chunk_size = chunk_size;
            return 1;
        }

        /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
        if (chunk_size == 0 && zero_size_end)
            return 0;

        offset += full_chunk_size ? chunk_size : 0x08 + chunk_size;
    }

    return 0;
}
int find_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0);
}
int find_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0);
}
int find_chunk(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, int full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian_size, int zero_size_end) {
    return find_chunk_internal(sf, chunk_id, start_offset, 0, full_chunk_size, out_chunk_offset, out_chunk_size, 1, big_endian_size, zero_size_end);
}
int find_chunk_riff_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 0, 0);
}
int find_chunk_riff_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 1, 0);
}
int find_chunk_riff_ve(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size, int big_endian) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, big_endian, big_endian, 0);
}
