/*
 * plugins.h - helper for plugins
 */
#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "streamfile.h"
#define TAG_LINE_MAX 2048

//todo improve API and make opaque
//typedef struct VGMSTREAM_TAGS VGMSTREAM_TAGS;
typedef struct {
    /* extracted output */
    char key[TAG_LINE_MAX];
    char val[TAG_LINE_MAX];

    /* file to find tags for */
    char targetname[TAG_LINE_MAX];

    /* tag section for filename (see comments below) */
    int section_found;
    off_t section_start;
    off_t section_end;
    off_t offset;

    /* commands */
    int autotrack_on;
    int autotrack_written;
    int track_count;
} VGMSTREAM_TAGS;



/* Extracts next valid tag in tagfile to *tag. Returns 0 if no more tags are found (meant to be
 * called repeatedly until 0). Key/values are trimmed and values can be in UTF-8. */
int vgmstream_tags_next_tag(VGMSTREAM_TAGS* tag, STREAMFILE* tagfile);

/* resets tagfile to restart reading from the beginning for a new filename */
void vgmstream_tags_reset(VGMSTREAM_TAGS* tag, const char* target_filename);

#endif /* _PLUGINS_H_ */
