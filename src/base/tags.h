#ifndef _TAGS_H_
#define _TAGS_H_

#include "../streamfile.h"


/* ****************************************** */
/* TAGS: loads key=val tags from a file       */
/* ****************************************** */

/* opaque tag state */
typedef struct VGMSTREAM_TAGS VGMSTREAM_TAGS;

/* Initializes TAGS and returns pointers to extracted strings (always valid but change
 * on every vgmstream_tags_next_tag call). Next functions are safe to call even if this fails (validate NULL).
 * ex.: const char *tag_key, *tag_val; tags=vgmstream_tags_init(&tag_key, &tag_val); */
VGMSTREAM_TAGS* vgmstream_tags_init(const char* *tag_key, const char* *tag_val);

/* Resets tagfile to restart reading from the beginning for a new filename.
 * Must be called first before extracting tags. */
void vgmstream_tags_reset(VGMSTREAM_TAGS* tags, const char* target_filename);


/* Extracts next valid tag in tagfile to *tag. Returns 0 if no more tags are found (meant to be
 * called repeatedly until 0). Key/values are trimmed and values can be in UTF-8. */
int vgmstream_tags_next_tag(VGMSTREAM_TAGS* tags, STREAMFILE* tagfile);

/* Closes tag file */
void vgmstream_tags_close(VGMSTREAM_TAGS* tags);

#endif
