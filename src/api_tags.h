#ifndef _API_TAGS_H_
#define _API_TAGS_H_
#include "api.h"
#if LIBVGMSTREAM_ENABLE

/* vgmstream's !tags.m3u API.
 * Doesn't need a main libvgmstream as tags aren't tied to loaded songs.
 *
 * Meant to be a simple implementation; feel free to ignore and roll your own
 * (or use another external tags plugin).
 */


/* tag state */
typedef struct {
    void* priv;                             // internal data

    const char* key;                        // current key
    const char* val;                        // current value
} libvgmstream_tags_t;

/* Initializes tags.
 * - libsf should point to a !tags.m3u file
 * - unlike libvgmstream_open, sf tagfile must be valid during the tag extraction process.
 */
LIBVGMSTREAM_API libvgmstream_tags_t* libvgmstream_tags_init(libvgmstream_streamfile_t* libsf);


/* Finds tags for a new filename. Must be called first before extracting tags.
 */
LIBVGMSTREAM_API void libvgmstream_tags_find(libvgmstream_tags_t* tags, const char* target_filename);


/* Extracts next valid tag in tagfile to key/val.
 * - returns false if no more tags are found (meant to be called repeatedly until false)
 * - key/values are trimmed of beginning/end whitespaces and values are in UTF-8
 */
LIBVGMSTREAM_API bool libvgmstream_tags_next_tag(libvgmstream_tags_t* tags);


/* Closes tags. */
LIBVGMSTREAM_API void libvgmstream_tags_free(libvgmstream_tags_t* tags);

#endif
#endif
