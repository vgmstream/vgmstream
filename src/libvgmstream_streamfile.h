#ifndef _LIBVGMSTREAM_STREAMFILE_H_
#define _LIBVGMSTREAM_STREAMFILE_H_
#include "libvgmstream.h"

/* vgmstream's IO API, defined as a "streamfile" ('SF').
 *
 * vgmstream mostly assumes there is an underlying filesystem (as usual in games), plus given video game formats are
 * often ill-defined it needs extra ops to handle edge cases: seeking + reading from arbitrary offsets, opening companion
 * files, filename/size tests, etc.
 *
 * If your case is too different you may still create a partial streamfile: returning a fake filename, only handling "open"
 * that reopens itself (same filename), etc. Simpler formats should work fine.
 */


// should be "libvgmstream_streamfile_t" but it was getting unwieldly
typedef struct libstreamfile_t {
    //uint32_t flags;   // info flags for vgmstream
    void* user_data;    // any internal structure

    /* read 'length' data at 'offset' to 'dst'
     * - assumes 0 = failure/EOF
     * - note that vgmstream needs to seek + read from arbitrary offset (non-linearly) fairly often
     */
    int (*read)(void* user_data, uint8_t* dst, int64_t offset, int length);

    /* get max offset (typically for checks or sample calculations)
     */
    int64_t (*get_size)(void* user_data);

    /* get current filename
     * - used to open same or other streamfiles and heuristics; no need to be a real path but extension matters
     */
    const char* (*get_name)(void* user_data);

    /* open another streamfile from filename (may be some path/protocol, or same as current get_name = reopen)
     * - vgmstream opens stuff based on current get_name (relative), so there shouldn't be need to transform this path
     */
    struct libstreamfile_t* (*open)(void* user_data, const char* filename);

    /* free current SF
     */
    void (*close)(struct libstreamfile_t* libsf);

} libstreamfile_t;


/* helpers */
static inline void libstreamfile_close(libstreamfile_t* libsf) {
    if (!libsf || !libsf->close)
        return;
    libsf->close(libsf);
}

/* base libstreamfile using STDIO (cached) */
LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_from_stdio(const char* filename);

/* base libstreamfile using a FILE (cached); the filename is needed as metadata */
LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_from_file(void* file, const char* filename);

 /* cached streamfile (recommended to wrap your external libsf since vgmstream needs to seek a lot) */
LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_buffered(libstreamfile_t* ext_libsf);

#endif
