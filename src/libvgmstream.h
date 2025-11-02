#ifndef _LIBVGMSTREAM_H_
#define _LIBVGMSTREAM_H_


/* libvgmstream: vgmstream's public API
 *
 * Basic usage (also see api_example.c):
 *   - libvgmstream_init(...)           // create context
 *   - libvgmstream_setup(...)          // setup config (if needed)
 *   - libvgmstream_open_stream(...)    // open format
 *   - libvgmstream_render(...)         // main decode
 *   - output samples + repeat libvgmstream_render until 'done' is set
 *   - libvgmstream_free(...)           // cleanup
 *
 * By default vgmstream behaves like a decoder (returns samples until stream end), but you can configure
 * it to loop N times or even downmix. In other words, it also behaves a bit like a player.
 * It exposes multiple convenience stuff mainly for various plugins with similar features.
 * This may make the API a bit odd, will probably improve later. Probably.
 *
 * Notes:
 * - now there is an API, internals (vgmstream.h) will change in the future so avoid accesing them
 * - some details described in the API may not happen at the moment (defined for future changes)
 * - uses long-winded libvgmstream_* names since internals already use the vgmstream_* 'namespace', #define if needed
 * - c-strings should be in UTF-8
 */


/*****************************************************************************/
/* DEFINES */

///* standard C param call and name mangling (to avoid __stdcall / .defs) */
//#define LIBVGMSTREAM_CALL __cdecl //needed?
//LIBVGMSTREAM_API (type) LIBVGMSTREAM_CALL libvgmstream_function(...);

/* external function behavior (for compile time) */
#if defined(LIBVGMSTREAM_EXPORT)
    #define LIBVGMSTREAM_API __declspec(dllexport) /* when exporting/creating vgmstream DLL */
#elif defined(LIBVGMSTREAM_IMPORT)
    #define LIBVGMSTREAM_API __declspec(dllimport) /* when importing/linking vgmstream DLL */
#else
    #define LIBVGMSTREAM_API /* nothing, internal/default */
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "libvgmstream_streamfile.h"


/*****************************************************************************/
/* VERSION */

/* Current API version, for static checks.
 * - only refers to the API itself, changes related to formats/etc don't alter this
 * - vgmstream's features are mostly stable, but this API may be tweaked from time to time
 */
#define LIBVGMSTREAM_API_VERSION_MAJOR 0x01    // breaking API/ABI changes
#define LIBVGMSTREAM_API_VERSION_MINOR 0x00    // compatible API/ABI changes
#define LIBVGMSTREAM_API_VERSION_PATCH 0x00    // fixes

/* Current API version, for dynamic checks. returns hex value: 0xMMmmpppp = MM-major, mm-minor, pppp-patch
 * - use when loading vgmstream as a dynamic library to ensure API/ABI compatibility
 */
LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void);

/* CHANGELOG:
 * - 1.0.0: initial version
 */


/*****************************************************************************/
/* DECODE */

/* available sample formats, interleaved: buf[0]=ch0, buf[1]=ch1, buf[2]=ch0, buf[3]=ch0, ... */
typedef enum {
    LIBVGMSTREAM_SFMT_PCM16 = 1,
    LIBVGMSTREAM_SFMT_PCM24 = 2,
    LIBVGMSTREAM_SFMT_PCM32 = 3,
    LIBVGMSTREAM_SFMT_FLOAT = 4,
} libvgmstream_sfmt_t;

/* current song info, may be copied around (values are info-only) */
typedef struct {
    /* main (always set) */
    int channels;                           // output channels
    int sample_rate;                        // output sample rate
    libvgmstream_sfmt_t sample_format;      // output buffer's sample type
    int sample_size;                        // derived from sample_type (pcm16=0x02, float=0x04, etc)

    /* extra info (may be 0 if not known or not relevant) */
    uint32_t channel_layout;                // standard WAVE bitflags, 0 if unset or non-standard

    int subsong_index;                      // 0 = none, N = loaded subsong N (1=first)
    int subsong_count;                      // 0 = format has no concept of subsongs, N = has N subsongs
                                            // ** 1 = format has subsongs, and only 1 for current file

    int input_channels;                     // original file's channels before downmixing (if any)
    //int interleave;                       // when file is interleaved
    //int interleave_first;                 // when file is interleaved
    //int interleave_last;                  // when file is interleaved
    //int frame_size;                       // when file has some configurable frame size

    /* sample info (may not be used depending on config) */
    int64_t stream_samples;                 // file's max samples (not final play duration)
    int64_t loop_start;                     // loop start sample
    int64_t loop_end;                       // loop end sample
    bool loop_flag;                         // if file loops
                                            // ** false + defined loops means looping was forcefully disabled
                                            // ** true + undefined loops means the file loops in a way not representable by loop points
    //bool rough_samples;                   // signal cases where loop points or sample count can't exactly reflect actual behavior

    bool play_forever;                      // if file loops forever based on current config (meaning _play never stops)
    int64_t play_samples;                   // totals after all calculations (after applying loop/fade/etc config)
                                            // ** may not be 100% accurate in some cases (check decoder's 'done' flag to stop)
                                            // ** if play_forever is set this is still provided for reference based on non-forever config

    int stream_bitrate;                     // average bitrate of the subsong (slightly bloated vs codec_bitrate; incorrect in rare cases)
    //int codec_bitrate;                    // average bitrate of the codec data [not possible / slow to calculate in most cases]

    /* descriptions */
    char codec_name[128];                   // represention of main decoder name
    char layout_name[128];                  // represention of how data is laid out
    char meta_name[128];                    // represention of format's name (not internal "tag" metadata)
    char stream_name[256];                  // stream's internal name or representation (not an internal filename not always useful)
    // ** these are a bit big for a struct, but the typical use case of vgmstream is opening a file > immediately
    //    query description and since libvgmstream returns its own copy it shouldn't be too much of a problem
    // ** (may be separated later)

    int format_id;                          // current format's ID (can be set when reopening streams to load a particular format)
                                            // ** this value WILL change without warning between vgmstream versions/commits

} libvgmstream_format_t;

/* current decoder state */
typedef struct {
    void* buf;                              // current decoded buf (valid after _decode until next call; may change between calls)
    int buf_samples;                        // current buffer samples (0 is possible in some cases)
    int buf_bytes;                          // current buffer bytes (channels * sample_size * samples)

    bool done;                              // when stream is done, based on config
                                            // ** note that with play_forever this flag is never set
} libvgmstream_decoder_t;

/* vgmstream context/handle */
typedef struct {
    void* priv;                             // internal data

    /* pointers for easier ABI compatibility */
    const libvgmstream_format_t* format;    // current song info, updated on _open
    libvgmstream_decoder_t* decoder;        // updated on each _decode call

} libvgmstream_t;



/* Inits the vgmstream context
 * - returns NULL on error
 */
LIBVGMSTREAM_API libvgmstream_t* libvgmstream_init(void);

/* Frees the vgmstream context and any other internal stuff.
 */
LIBVGMSTREAM_API void libvgmstream_free(libvgmstream_t* lib);


/* configures how vgmstream behaves internally when playing a file */
typedef struct {

    bool disable_config_override;           // ignore forced (TXTP) config
    bool allow_play_forever;                // must allow manually as some cases a TXTP may set loop forever but client may not handle it

    bool play_forever;                      // keeps looping forever (file must have loop_flag set)
    bool ignore_loop;                       // ignores loops points
    bool force_loop;                        // enables full loops (0..samples) if file doesn't have loop points
    bool really_force_loop;                 // forces full loops (0..samples) even if file has loop points
    bool ignore_fade;                       // don't fade after N loops and play remaning stream (for files with outros)

    double loop_count;                      // target loops (values like 1.5 are ok); defaults to 1; set to -1 to remove the loop section
    double fade_time;                       // fade period after target loops
    double fade_delay;                      // fade delay after target loops

    int stereo_track;                       // forces vgmstream to decode one 2ch+2ch+2ch... 'track' and discard other channels, where 0 = disabled, 1..N = Nth track
    int auto_downmix_channels;              // downmix if vgmstream's channels are higher than value
                                            // ** for players that can only handle N channels
                                            // ** this type of downmixing is very simplistic and not recommended

    libvgmstream_sfmt_t force_sfmt;         // forces output buffer to be remixed into some sample format

  //int format_id;                          // force a format (for example when loading new subsong of the same archive, for a minuscule speed up)
  //                                        // ** only applies when called before _open_stream

} libvgmstream_config_t;

/* optionally pass config to apply to next _open_stream (or current stream if already loaded and not setup previously)
 * - some settings may be ignored in invalid or complex cases (ex. TXTP with pre-configured options)
 * - once config is applied to current stream new _setup calls only apply to next _open_stream
 * - pass NULL to clear current config
 * - remember config may change format info like channels or output format (recheck if calling after loading song)
 */
LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg);


/* Opens file based on config and prepares it to play if supported.
 * - returns < 0 on error (file not recognised, invalid subsong index, etc)
 * - will close currently loaded song if needed
 * - libsf (custom IO) is not needed after _open and should be closed, as vgmstream re-opens as needed
 * - subsong can be 1..N or 0 = default/first
 *   ** to check if a file has subsongs, _open default + check format->total_subsongs (then _open Nth)
 */
LIBVGMSTREAM_API int libvgmstream_open_stream(libvgmstream_t* lib, libstreamfile_t* libsf, int subsong);

/* Closes current song; may still use libvgmstream to open other songs
 */
LIBVGMSTREAM_API void libvgmstream_close_stream(libvgmstream_t* lib);


/* Decodes next batch of samples
 * - vgmstream supplies its own buffer, updated on lib->decoder->* values (may change between calls)
 * - on last call will return samples + set flag lib->decoder->done (must handle last buf before quitting)
 * - returns < 0 on error
 */
LIBVGMSTREAM_API int libvgmstream_render(libvgmstream_t* lib);

/* Same as _play, but fills some external buffer (also updates lib->decoder->* values)
 * - returns < 0 on error
 * - buf must be at least as big as channels * sample_size * buf_samples
 * - note that may return less than requested samples (such as near EOF)
 * - needs copying around from internal bufs so may be slightly slower; mainly for cases when you have buf constraints
 */
LIBVGMSTREAM_API int libvgmstream_fill(libvgmstream_t* lib, void* buf, int buf_samples);

/* Gets current position within the song.
 * - return < 0 on error
 */
LIBVGMSTREAM_API int64_t libvgmstream_get_play_position(libvgmstream_t* lib);

/* Seeks to absolute position. Will clamp incorrect values such as seeking before/past playable length.
 */
LIBVGMSTREAM_API void libvgmstream_seek(libvgmstream_t* lib, int64_t sample);

/* Reset current song
 */
LIBVGMSTREAM_API void libvgmstream_reset(libvgmstream_t* lib);


/* Helper: calls _init + _setup + _open_stream
 */
LIBVGMSTREAM_API libvgmstream_t* libvgmstream_create(libstreamfile_t* libsf, int subsong, libvgmstream_config_t* cfg);


/*****************************************************************************/
/* HELPERS */

typedef enum {
    LIBVGMSTREAM_LOG_LEVEL_ALL      = 0,
    LIBVGMSTREAM_LOG_LEVEL_DEBUG    = 20,
    LIBVGMSTREAM_LOG_LEVEL_INFO     = 30,
    LIBVGMSTREAM_LOG_LEVEL_NONE     = 100,
} libvgmstream_loglevel_t;

/* Defines a global log callback, as vgmstream sometimes communicates format issues to the user.
 * - note that log is currently set globally rather than per libvgmstream_t
 * - call with LOG_LEVEL_NONE to disable current callback
 * - call with NULL callback to use default stdout callback
*/
LIBVGMSTREAM_API void libvgmstream_set_log(libvgmstream_loglevel_t level, void (*callback)(int level, const char* str));


/* Returns a list of supported extensions (WARNING: it's pretty big), such as "adx", "dsp", etc.
 * Mainly for plugins that want to know which extensions are supported.
 * - returns NULL if no size is provided
 */
LIBVGMSTREAM_API const char** libvgmstream_get_extensions(int* size);

/* Same as above, buf returns a list what vgmstream considers "common" formats (such as "wav", "ogg"),
 * which usually one doesn't want to associate to vgmstream.
 * - returns NULL if no size is provided
 */
LIBVGMSTREAM_API const char** libvgmstream_get_common_extensions(int* size);


typedef struct {
    bool is_extension;           /* set if filename is just an extension (otherwise may be seen as 'extensionless') */
    bool skip_standard;           /* disable extension check vs default formats */
    bool reject_extensionless;   /* enable if player can't play extensionless files */
    bool accept_unknown;         /* enable to allow any extension even if not known by vgmstream (for .txth) */
    bool accept_common;          /* enable to allow known-but-common extension (when player has plugin priority) */
} libvgmstream_valid_t;

/* Returns if vgmstream can parse a filename by extension, to reject some files earlier.
 * - doesn't check file contents (that's only done on _open)
 * - config may be NULL
 * - mainly for plugins that want to fail early; libvgmstream doesn't use this
 */
LIBVGMSTREAM_API bool libvgmstream_is_valid(const char* filename, libvgmstream_valid_t* cfg);


typedef struct {
    bool force_title;           // force stream name as title (by default it may be hiddedn if file has no subsongs)
    bool subsong_range;         // print a range of possible subsongs after title 'filename#1~N'
    bool remove_extension;      // remove extension from passed filename
    bool remove_archive;        // remove '(archive)|(subfile)' format of some plugins
    const char* filename;       // base file's filename
                                // ** note that sometimes vgmstream doesn't have/know the original name, so it's needed again here
} libvgmstream_title_t;

/* Get a simple title for plugins, derived from internal stream name if available
 * - valid after _open
 */
LIBVGMSTREAM_API int libvgmstream_get_title(libvgmstream_t* lib, libvgmstream_title_t* cfg, char* buf, int buf_len);

/* Writes a description of the current song into dst. Will always be null-terminated.
 * - returns < 0 if file was truncated, though will still succeed.
 */
LIBVGMSTREAM_API int libvgmstream_format_describe(libvgmstream_t* lib, char* dst, int dst_size);

/* Return true if vgmstream detects from the filename that file can be used even if doesn't physically exist.
 */
LIBVGMSTREAM_API bool libvgmstream_is_virtual_filename(const char* filename);


/*****************************************************************************/
/* TAGS */

/* Meant to be a simple implementation; feel free to ignore and roll your own (or use another tags plugin).
 * Doesn't need a main libvgmstream as tags aren't tied to loaded songs. */

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
LIBVGMSTREAM_API libvgmstream_tags_t* libvgmstream_tags_init(libstreamfile_t* libsf);

/* Finds tags for a new filename. Must be called first before extracting tags.
 */
LIBVGMSTREAM_API void libvgmstream_tags_find(libvgmstream_tags_t* tags, const char* target_filename);

/* Extracts next valid tag in tagfile to key/val.
 * - returns false if no more tags are found (meant to be called repeatedly until false)
 * - key/values are trimmed of beginning/end whitespaces and values are UTF-8
 */
LIBVGMSTREAM_API bool libvgmstream_tags_next_tag(libvgmstream_tags_t* tags);

/* Closes tags.
 * - passed libsf is not closed
 */
LIBVGMSTREAM_API void libvgmstream_tags_free(libvgmstream_tags_t* tags);


#endif
