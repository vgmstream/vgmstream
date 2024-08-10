#ifndef _LIBVGMSTREAM_STREAMFILE_H_
#define _LIBVGMSTREAM_STREAMFILE_H_
#include "libvgmstream.h"
#if LIBVGMSTREAM_ENABLE

/* vgmstream's IO API, defined as a "streamfile" (SF).
 * 
 * vgmstream roughly assumes there is an underlying filesystem (as usual in games): seeking + reading from arbitrary offsets,
 * opening companion files, filename tests, etc. If your case is too different you may still create a partial streamfile: returning
 * a fake filename, only handling "open" that reopens itself (same filename), etc. Simpler formats will probably work just fine.
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

    /* read 'length' data at internal offset to 'dst'
     * - assumes 0 = failure/EOF
     */
    int (*read)(void* user_data, uint8_t* dst, int dst_size);

    /* seek to offset
     * - note that vgmstream needs to seek + read fairly often (to be optimized later)
     */
    int64_t (*seek)(void* user_data, int64_t offset, int whence);

    /* get max offset (typically for checks or calculations)
     */
    int64_t (*get_size)(void* user_data);

    /* get current filename (used to open same or other streamfiles and heuristics; no need to be a real path)
     */
    const char* (*get_name)(void* user_data);

    /* open another streamfile from filename (may be some path/protocol, or same as current get_name = reopen)
     * - vgmstream opens stuff based on current get_name (relative), so there shouldn't be need to transform this path
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


LIBVGMSTREAM_API libvgmstream_streamfile_t* libvgmstream_streamfile_open_from_stdio(const char* filename);

#endif
#endif
