#ifndef _API_STREAMFILE_H_
#define _API_STREAMFILE_H_
#include "api.h"
#if LIBVGMSTREAM_ENABLE

/* vgmstream's IO API, defined as a "streamfile" (SF).
 * 
 * Compared to typical IO, vgmstream has some extra needs that roughly assume there is an underlying filesystem (as usual in games):
 * - seeking + reading from arbitrary offsets: header not in the beginning of a stream, rewinding back when looping, etc
 * - opening other streamfiles: reopening a copy of the current SF, formats with split header + data, decryption files, etc
 * - extracting the filename: opening similarly named companion files, basic extension sanity checks, heuristics for odd cases, etc
 * 
 * If your IO can't fully satisfy those constraints, it may still be possible to create a streamfile that just simulates part of it.
 * For example, returning a fake filename, and only handling "open" that reopens itself (same filename), while returning default/incorrect
 * values for non-handled operations. Simpler formats will probably work just fine.
 */


enum {
    LIBVGMSTREAM_STREAMFILE_SEEK_SET            = 0,
    LIBVGMSTREAM_STREAMFILE_SEEK_CUR            = 1,
    LIBVGMSTREAM_STREAMFILE_SEEK_END            = 2,
  //LIBVGMSTREAM_STREAMFILE_SEEK_GET_OFFSET     = 3,
  //LIBVGMSTREAM_STREAMFILE_SEEK_GET_SIZE       = 5,
};

typedef struct libvgmstream_streamfile_t {
    //uint32_t flags;   // info flags for vgmstream
    void* user_data;    // any internal structure

    /* read 'length' data at internal offset to 'dst' (implicit seek if needed)
     * - assumes 0 = failure/EOF 
     */
    int (*read)(void* user_data, uint8_t* dst, int dst_size);

    /* seek to offset
     * - note that due to how vgmstream works this is a fairly common operation (to be optimized later)
     */
    int64_t (*seek)(void* user_data, int64_t offset, int whence);

    /* get max offset
     */
    int64_t (*get_size)(void* user_data);

    /* get current filename
     */
    const char* (*get_name)(void* user_data);

    /* open another streamfile from filename (may be some path/protocol, or same as current get_name = reopen)
     * - vgmstream mainly opens stuff based on current get_name (relative), so there shouldn't be need to transform this path
     */
    struct libvgmstream_streamfile_t* (*open)(void* user_data, const char* filename);

    /* free current SF (needed for copied streamfiles) */
    void (*close)(struct libvgmstream_streamfile_t* libsf);

} libvgmstream_streamfile_t;


/* helper */
static inline void libvgmstream_streamfile_close(libvgmstream_streamfile_t* libsf) {
    if (!libsf || !libsf->close)
        return;
    libsf->close(libsf);
}


LIBVGMSTREAM_API libvgmstream_streamfile_t* libvgmstream_streamfile_from_filename(const char* filename);

#endif
#endif
