/*
 * plugins.h - helper for plugins
 */
#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "../streamfile.h"
#include "../vgmstream.h"


/* List supported formats and return elements in the list, for plugins that need to know.
 * The list disables some common formats that may conflict (.wav, .ogg, etc). */
const char** vgmstream_get_formats(size_t* size);

/* same, but for common-but-disabled formats in the above list. */
const char** vgmstream_get_common_formats(size_t* size);


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


typedef struct {
    int allow_play_forever;
    int disable_config_override;

    /* song mofidiers */
    int play_forever;           /* keeps looping forever (needs loop points) */
    int ignore_loop;            /* ignores loops points */
    int force_loop;             /* enables full loops (0..samples) if file doesn't have loop points */
    int really_force_loop;      /* forces full loops even if file has loop points */
    int ignore_fade;            /*  don't fade after N loops */

    /* song processing */
    double loop_count;          /* target loops */
    double fade_delay;          /* fade delay after target loops */
    double fade_time;           /* fade period after target loops */

  //int downmix;                /* max number of channels allowed (0=disable downmix) */

} vgmstream_cfg_t;

// WARNING: these are not stable and may change anytime without notice
void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* pcfg);
int32_t vgmstream_get_samples(VGMSTREAM* vgmstream);
int vgmstream_get_play_forever(VGMSTREAM* vgmstream);
void vgmstream_set_play_forever(VGMSTREAM* vgmstream, int enabled);


typedef struct {
    int force_title;
    int subsong_range;
    int remove_extension;
    int remove_archive;
} vgmstream_title_t;

/* get a simple title for plugins */
void vgmstream_get_title(char* buf, int buf_len, const char* filename, VGMSTREAM* vgmstream, vgmstream_title_t* cfg);

enum {
    VGM_LOG_LEVEL_INFO = 1,
    VGM_LOG_LEVEL_DEBUG = 2,
    VGM_LOG_LEVEL_ALL = 100,
};
// CB: void (*callback)(int level, const char* str)
void vgmstream_set_log_callback(int level, void* callback);
void vgmstream_set_log_stdout(int level);


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
void vgmstream_mixing_autodownmix(VGMSTREAM* vgmstream, int max_channels);

/* downmixes to get stereo from start channel */
void vgmstream_mixing_stereo_only(VGMSTREAM* vgmstream, int start);

/* sets a fadeout */
//void vgmstream_mixing_fadeout(VGMSTREAM *vgmstream, float start_second, float duration_seconds);

#endif /* _PLUGINS_H_ */
