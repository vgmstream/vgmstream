#include "chunks.h"
#include "reader_sf.h"
#include "endianness.h"


bool next_chunk(chunk_t* chunk, STREAMFILE* sf) {
    read_u32_t read_u32type = get_read_u32(!chunk->le_type);
    read_u32_t read_u32size = get_read_u32(chunk->be_size);

    // can be used to signal "stop"
    if (chunk->current == 0xFFFFFFFF)
        return false;

    if (chunk->max == 0)
        chunk->max = get_streamfile_size(sf);

    if (chunk->current >= chunk->max)
        return false;

    uint32_t remaining = chunk->max - chunk->current;
    if (remaining < 0x08)
        return false;

    chunk->type = read_u32type(chunk->current + 0x00,sf);
    chunk->size = read_u32size(chunk->current + 0x04,sf);

    // read past data
    if (chunk->type == 0xFFFFFFFF || chunk->size == 0xFFFFFFFF)
        return false;

    if (chunk->size > remaining - 0x08)
        return false;

    chunk->offset = chunk->current + 0x08;
    chunk->current += chunk->full_size ? chunk->size : 0x08 + chunk->size;
    //;VGM_LOG("CHUNK: %x, %x, %x\n", dc.offset, chunk->type, chunk->size);

    // enforce 16-bit chunk alignment as per spec, where chunk_size may be odd (0x11) but must be
    // set to even (0x12) (typically pre-padded though). At EOF may not use padding.
    if (chunk->alignment && (chunk->size & 0x01)) {
        if (chunk->current < chunk->max)
            chunk->current++;
    }

    // empty chunk with 0 size is ok, seen in some formats (XVAG uses it as end marker, Wwise in JUNK)
    if (chunk->type == 0 /*|| chunk->size == 0*/)
        return false;

    /* more chunks remain */
    return true;
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
static bool find_chunk_internal(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, bool big_endian_type, bool big_endian_size, bool zero_size_end, bool aligned) {
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
            return false;

        if (chunk_type == chunk_id) {
            if (out_chunk_offset) *out_chunk_offset = offset + 0x08;
            if (out_chunk_size) *out_chunk_size = chunk_size;
            return true;
        }

        /* empty chunk with 0 size, seen in some formats (XVAG uses it as end marker, Wwise doesn't) */
        if (chunk_size == 0 && zero_size_end)
            return false;

        /* next chunk should be on a 16-bit boundary (standard RIFF behavior) */
        if (aligned && (chunk_size & 0x01))
            chunk_size++;

        offset += full_chunk_size ? chunk_size : 0x08 + chunk_size;
    }

    return false;
}
bool find_aligned_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0, 1);
}
bool find_aligned_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0, 1);
}
bool find_chunk_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 1, 0, 0);
}
bool find_chunk_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk(sf, chunk_id, start_offset, full_chunk_size, out_chunk_offset, out_chunk_size, 0, 0, 0);
}
bool find_chunk(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, bool full_chunk_size, off_t *out_chunk_offset, size_t *out_chunk_size, bool big_endian_size, bool zero_size_end, bool aligned) {
    return find_chunk_internal(sf, chunk_id, start_offset, 0, full_chunk_size, out_chunk_offset, out_chunk_size, 1, big_endian_size, zero_size_end, aligned);
}
bool find_chunk_riff_le(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 0, 0, 0);
}
bool find_chunk_riff_be(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, 1, 1, 0, 0);
}
bool find_chunk_riff_ve(STREAMFILE* sf, uint32_t chunk_id, off_t start_offset, size_t max_size, off_t *out_chunk_offset, size_t *out_chunk_size, bool big_endian) {
    return find_chunk_internal(sf, chunk_id, start_offset, max_size, 0, out_chunk_offset, out_chunk_size, big_endian, big_endian, 0, 0);
}
