#if 0
#ifndef _API_TAGS_H_
#define _API_TAGS_H_
#include "api.h"
#include "api_streamfile.h"

/* vgmstream's !tags.m3u API.
 * Doesn't need a main libvgmstream lib as tags aren't tied to loaded songs.
 */

/* tag state */
typedef struct {
    void* priv;                             // internal data

    const char* key;                        // current key
    const char* val;                        // current value

} libvgmstream_tags_t;

/* Initializes tags.
 * - may be reused for different files for cache purposed
 */
LIBVGMSTREAM_API libvgmstream_tags_t* libvgmstream_tags_init();

/* Restarts tagfile for a new filename. Must be called first before extracting tags.
 * - unlike libvgmstream_open, sf tagfile must be valid during the tag extraction process.
 */
LIBVGMSTREAM_API void libvgmstream_tags_reset(libvgmstream_tags_t* tags, libvgmstream_streamfile_t* sf, const char* target_filename);

/* Extracts next valid tag in tagfile to key/val.
 * - returns 0 if no more tags are found (meant to be called repeatedly until 0)
 * - key/values are trimmed of beginning/end whitespaces and values are in UTF-8
 */
LIBVGMSTREAM_API int libvgmstream_tags_next_tag(libvgmstream_tags_t* tags);

/* Closes tags. */
LIBVGMSTREAM_API void libvgmstream_tags_free(libvgmstream_tags_t* tags);

#endif
#endif
