/*
 * plugins.h - helper for plugins
 */
#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "streamfile.h"


#if 0
/* ****************************************** */
/* PLAYER: simplifies plugin code             */
/* ****************************************** */

/* opaque player state */
typedef struct VGMSTREAM_PLAYER VGMSTREAM_PLAYER;

typedef struct {
    //...
} VGMSTREAM_PLAYER_INFO;

VGMSTREAM_PLAYER* vgmstream_player_init(...);

VGMSTREAM_PLAYER* vgmstream_player_format_check(...);
VGMSTREAM_PLAYER* vgmstream_player_set_format_whilelist(...);
VGMSTREAM_PLAYER* vgmstream_player_set_format_blacklist(...);

VGMSTREAM_PLAYER* vgmstream_player_set_file(...);

VGMSTREAM_PLAYER* vgmstream_player_get_config(...);

VGMSTREAM_PLAYER* vgmstream_player_set_config(...);

VGMSTREAM_PLAYER* vgmstream_player_get_buffer(...);

VGMSTREAM_PLAYER* vgmstream_player_get_info(...);

VGMSTREAM_PLAYER* vgmstream_player_describe(...);

VGMSTREAM_PLAYER* vgmstream_player_get_title(...);

VGMSTREAM_PLAYER* vgmstream_player_get_tagfile(...);

VGMSTREAM_PLAYER* vgmstream_player_play(...);

VGMSTREAM_PLAYER* vgmstream_player_seek(...);

VGMSTREAM_PLAYER* vgmstream_player_close(...);

#endif



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


/* ****************************************** */
/* MIXING: modifies vgmstream output          */
/* ****************************************** */

/* Enables mixing effects, with max outbuf samples as a hint. Once active, plugin
 * must use returned input_channels to create outbuf and output_channels to output audio.
 * max_sample_count may be 0 if you only need to query values and not actually enable it.
 * Needs to be enabled last after adding effects. */
void vgmstream_mixing_enable(VGMSTREAM* vgmstream, int32_t max_sample_count, int *input_channels, int *output_channels);

/* sets automatic downmixing if vgmstream's channels are higher than max_channels */
void vgmstream_mixing_autodownmix(VGMSTREAM *vgmstream, int max_channels);

/* sets a fadeout */
//void vgmstream_mixing_fadeout(VGMSTREAM *vgmstream, float start_second, float duration_seconds);

#endif /* _PLUGINS_H_ */
