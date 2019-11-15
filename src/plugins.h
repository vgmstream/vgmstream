/*
 * plugins.h - helper for plugins
 */
#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "streamfile.h"

/* ****************************************** */
/* CONTEXT: simplifies plugin code            */
/* ****************************************** */

typedef struct {
    int is_extension;           /* set if filename is already an extension */
    int skip_standard;          /* set if shouldn't check standard formats */
    int reject_extensionless;   /* set if player can't play extensionless files */
    int accept_unknown;         /* set to allow any extension (for txth) */
    int accept_common;          /* set to allow known-but-common extension (when player has plugin priority) */
} vgmstream_ctx_valid_cfg;

/* returns if vgmstream can parse file by extension */
int vgmstream_ctx_is_valid(const char* filename, vgmstream_ctx_valid_cfg *cfg);

#if 0

/* opaque player state */
typedef struct VGMSTREAM_CTX VGMSTREAM_CTX;

typedef struct {
    //...
} VGMSTREAM_CTX_INFO;

VGMSTREAM_CTX* vgmstream_ctx_init(...);

VGMSTREAM_CTX* vgmstream_ctx_format_check(...);
VGMSTREAM_CTX* vgmstream_ctx_set_format_whilelist(...);
VGMSTREAM_CTX* vgmstream_ctx_set_format_blacklist(...);

VGMSTREAM_CTX* vgmstream_ctx_set_file(...);

VGMSTREAM_CTX* vgmstream_ctx_get_config(...);

VGMSTREAM_CTX* vgmstream_ctx_set_config(...);

VGMSTREAM_CTX* vgmstream_ctx_get_buffer(...);

VGMSTREAM_CTX* vgmstream_ctx_get_info(...);

VGMSTREAM_CTX* vgmstream_ctx_describe(...);

VGMSTREAM_CTX* vgmstream_ctx_get_title(...);

VGMSTREAM_CTX* vgmstream_ctx_get_tagfile(...);

VGMSTREAM_CTX* vgmstream_ctx_play(...);

VGMSTREAM_CTX* vgmstream_ctx_seek(...);

VGMSTREAM_CTX* vgmstream_ctx_close(...);

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
