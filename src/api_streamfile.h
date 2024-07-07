#if 0
#ifndef _API_STREAMFILE_H_
#define _API_STREAMFILE_H_

/* vgmstream's IO API, defined as a "streamfile" (SF).
 * 
 * Unlike more typical IO, vgmstream has particular needs that roughly assume there is some underlying filesystem (as typical of games):
 * - reading from arbitrary offsets: header not found in the beginning of a stream, rewinding during looping, etc
 * - opening other streamfiles: reopening a copy of current SF, formats with split header + data, decryption files, etc
 * - filename: opening similarly named companion files, heuristics when file's data is not enough, etc
 * 
 * If your use case can't satisfy those constraints, it may still be possible to create a streamfile that just simulates part of it.
 * For example, loading data into memory, returning a fake filename, and only handling "open" that reopens itself (same filename),
 * while returning default/incorrect values on non-handled operations. Simpler, non-looped formats probably work fine just being read linearly.
 */

#include "api.h"

// TODO: pass opaque instead?
typedef struct libvgmstream_streamfile_t {
    /* user data */
    void* opaque;

    /* read 'length' data at 'offset' to 'dst' (implicit seek) */
    size_t (*read)(struct libvgmstream_streamfile_t* sf, uint8_t* dst, int64_t offset, size_t length);

    /* get max offset */
    size_t (*get_size)(struct libvgmstream_streamfile_t* sf); //TODO return int64_t?

    /* copy current filename to name buf */
    void (*get_name)(struct libvgmstream_streamfile_t* sf, char* name, size_t name_size);

    /* open another streamfile from filename (which may be some internal path/protocol) */
    struct libvgmstream_streamfile_t* (*open)(struct libvgmstream_streamfile_t* sf, const char* const filename, size_t buf_size);

    /* free current STREAMFILE */
    void (*close)(struct libvgmstream_streamfile_t* sf);

} libvgmstream_streamfile_t;

/* helper */
static inline void libvgmstream_streamfile_close(libvgmstream_streamfile_t* sf) {
    if (!sf)
        return;
    sf->close(sf);
}

//LIBVGMSTREAM_API libvgmstream_streamfile_t* libvgmstream_streamfile_get_file(const char* filename);

#endif
#endif
