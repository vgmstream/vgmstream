#ifndef _API_H_
#define _API_H_

#include "base/plugins.h"


//possible future public/opaque API
#if 0

#include <stdint.h>
#include "api_streamfile.h"

/* Current API version (major=breaking API/ABI changes, minor=compatible ABI changes). 
 * Internal bug fixes or added formats don't change these (see commit revision).
 * Regular vgmstream features or formats are stable and are rarely removed, while this API may change from time to time */
#define LIBVGMSTREAM_API_VERSION_MAJOR 0
#define LIBVGMSTREAM_API_VERSION_MINOR 0

/* define standard C param call and name mangling (to avoid __stdcall / .defs) */
//#define LIBVGMSTREAM_CALL __cdecl //needed?

/* define external function types (during compilation) */
//LIBVGMSTREAM_API void LIBVGMSTREAM_CALL vgmstream_function(void);
#if defined(LIBVGMSTREAM_EXPORT)
    #define LIBVGMSTREAM_API __declspec(dllexport) /* when exporting/creating vgmstream DLL */
#elif defined(LIBVGMSTREAM_IMPORT)
    #define LIBVGMSTREAM_API __declspec(dllimport) /* when importing/linking vgmstream DLL */
#else
    #define LIBVGMSTREAM_API /* nothing, internal/default */
#endif

/* opaque vgmstream context/handle */
typedef struct libvgmstream_t libvgmstream_t;

/* init base vgmstream context */
libvgmstream_t* libvgmstream_init(void);

typedef struct {
    int downmix_max_channels;   // max number of channels
    //int upmix_min_channels;     // adds channels until min
} libvgmstream_config_t;

/* pass default config, that will be applied to song on open (some formats like TXTP may override
 * these settings).
 * May only be called without song loaded (before _open or after _close), otherwise ignored.  */
void libvgmstream_setup(libvgmstream_t* vctx, libvgmstream_config_t* vcfg);

//void libvgmstream_buffer(libvgmstream_t* vctx, int samples, int max_samples);

/* Opens a new STREAMFILE to play. Returns < 0 on error when the file isn't recogniced.
 * If file has subsongs, first open usually loads first subsong. get_info then can be used to check
 * whether file has more subsongs (total_subsongs > 1), and call others.
 *  */
int libvgmstream_open(libvgmstream_t* vctx, STREAMFILE* sf);
int libvgmstream_open_subsong(libvgmstream_t* vctx, STREAMFILE* sf, int subsong);

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
} libvgmstream_into_t;

/* Get info from current song. */
void libvgmstream_t_get_info(libvgmstream_t* vctx, libvgmstream_into_t* vinfo);


libvgmstream_sbuf_t* libgstream_get_sbuf(libvgmstream_t* vctx);

/* Converts samples. returns number of rendered samples, or <=0 if no more
 * samples left (will fill buffer with silence) */
int libvgmstream_play(libvgmstream_t* vctx);



/* Gets final time based on config and current song. If config is set to "play forever"
 * this still returns final time based on config as a reference. Returns > 0 on success. */
int32_t libvgmstream_get_total_time(libvgmstream_t* vctx);
double libvgmstream_get_total_samples(libvgmstream_t* vctx);


/* Gets current position within song. When "play forever" is set, it'll clamp results to total_time. */
int32_t libvgmstream_get_current_time(libvgmstream_t* vctx);
double libvgmstream_get_current_samples(libvgmstream_t* vctx);


/* Seeks to position */
libvgmstream_t* libvgmstream_seek_absolute_sample(libvgmstream_t* vctx, int32_t sample);
libvgmstream_t* libvgmstream_seek_absolute_time(libvgmstream_t* vctx, double time);
libvgmstream_t* libvgmstream_seek_current_sample(libvgmstream_t* vctx, int32_t sample);
libvgmstream_t* libvgmstream_seek_current_time(libvgmstream_t* vctx, double time);


/* Closes current song. */
void libvgmstream_close(libvgmstream_t* vctx);

/* Frees vgmstream context. */
void libvgmstream_free(libvgmstream_t* vctx);

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
#endif
