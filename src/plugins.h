/*
 * plugins.h - helper for plugins
 */
#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "streamfile.h"
//todo rename to api.h once public enough


#if 0
/* define standard C param call and name mangling (to avoid __stdcall / .defs) */
//#define VGMSTREAM_CALL __cdecl //needed?

/* define external function types (during compilation) */
#if defined(VGMSTREAM_EXPORT)
    #define VGMSTREAM_API __declspec(dllexport) /* when exporting/creating vgmstream DLL */
#elif defined(VGMSTREAM_IMPORT)
    #define VGMSTREAM_API __declspec(dllimport) /* when importing/linking vgmstream DLL */
#else
    #define VGMSTREAM_API /* nothing, internal/default */
#endif

//VGMSTREAM_API void VGMSTREAM_CALL vgmstream_function(void);
#endif


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


#if 0
//possible future public/opaque API

/* opaque player state */
//#define VGMSTREAM_CTX_VERSION 1
typedef struct VGMSTREAM_CTX VGMSTREAM_CTX;


/* Setups base vgmstream player context. */
VGMSTREAM_CTX* vgmstream_init_ctx(void);


/* Sets default config, that will be applied to song on open (some formats like TXTP may override
 * these settings).
 * May only be called without song loaded (before _open or after _close), otherwise ignored.  */
void vgmstream_set_config(VGMSTREAM_CTX* vctx, VGMSTREAM_CFG* vcfg);

void vgmstream_set_buffer(VGMSTREAM_CTX* vctx, int samples, int max_samples);

/* Opens a new STREAMFILE to play. Returns < 0 on error when the file isn't recogniced.
 * If file has subsongs, first open usually loads first subsong. get_info then can be used to check
 * whether file has more subsongs (total_subsongs > 1), and call others.
 *  */
int vgmstream_open(STREAMFILE* sf);
int vgmstream_open_subsong(STREAMFILE* sf, int subsong);

typedef struct {
    const int channels;
    const int sample_rate;

    const int sample_count;         /* file's samples (not final duration) */
    const int loop_start_sample;
    const int loop_end_sample;
    const int loop_flag;

    const int current_subsong;      /* 0=not set, N=loaded subsong N */
    const int total_subsongs;       /* 0=format has no subsongs, N=has N subsongs */
    const int file_bitrate;         /* file's average bitrate */
    //const int codec_bitrate;      /* codec's average bitrate */

    /* descriptions */
    //const char* codec;
    //const char* layout;
    //const char* metadata;

    //int type;                     /* 0=pcm16, 1=float32, always interleaved: [0]=ch0, [1]=ch1 ... */
} VGMSTREAM_INFO;

/* Get info from current song. */
void vgmstream_ctx_get_info(VGMSTREAM_CTX* vctx, VGMSTREAM_INFO* vinfo);


/* Gets final time based on config and current song. If config is set to "play forever"
 * this still returns final time based on config as a reference. Returns > 0 on success. */
int32_t vgmstream_get_total_time(VGMSTREAM_CTX* vctx);
double vgmstream_get_total_samples(VGMSTREAM_CTX* vctx);


/* Gets current position within song. When "play forever" is set, it'll clamp results to total_time. */
int32_t vgmstream_get_current_time(VGMSTREAM_CTX* vctx);
double vgmstream_get_current_samples(VGMSTREAM_CTX* vctx);


/* Seeks to position */
VGMSTREAM_CTX* vgmstream_seek_absolute_sample(VGMSTREAM_CTX* vctx, int32_t sample);
VGMSTREAM_CTX* vgmstream_seek_absolute_time(VGMSTREAM_CTX* vctx, double time);
VGMSTREAM_CTX* vgmstream_seek_current_sample(VGMSTREAM_CTX* vctx, int32_t sample);
VGMSTREAM_CTX* vgmstream_seek_current_time(VGMSTREAM_CTX* vctx, double time);


/* Closes current song. */
void vgmstream_close(VGMSTREAM_CTX* vctx);

/* Frees vgmstream context. */
void vgmstream_free_ctx(VGMSTREAM_CTX* vctx);


/* Converts samples. returns number of rendered samples, or <=0 if no more
 * samples left (will fill buffer with silence) */
int  vgmstream_play(VGMSTREAM_CTX* vctx);


#if 0
void vgmstream_get_buffer(...);

void vgmstream_format_check(...);
void vgmstream_set_format_whilelist(...);
void vgmstream_set_format_blacklist(...);

const char* vgmstream_describe(...);

const char* vgmstream_get_title(...);

VGMSTREAM_TAGS* vgmstream_get_tagfile(...);
#endif

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
void vgmstream_mixing_autodownmix(VGMSTREAM* vgmstream, int max_channels);

/* downmixes to get stereo from start channel */
void vgmstream_mixing_stereo_only(VGMSTREAM* vgmstream, int start);

/* sets a fadeout */
//void vgmstream_mixing_fadeout(VGMSTREAM *vgmstream, float start_second, float duration_seconds);

#endif /* _PLUGINS_H_ */
