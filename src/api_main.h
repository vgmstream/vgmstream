#if 0
#ifndef _API_MAIN_H_
#define _API_MAIN_H_
#include "api.h"
#include "api_streamfile.h"

/* vgmstream's main (decode) API.
 */

/* returns API version in hex format: 0xMMmmpppp = MM-major, mm-minor, pppp-patch
 * - use when loading vgmstream as a dynamic library to ensure API/ABI compatibility */
LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void);

// interleaved: buf[0]=ch0, buf[1]=ch1, buf[2]=ch0, buf[3]=ch0, ...
enum { 
    LIBVGMSTREAM_SAMPLE_PCM16   = 0x01,
    LIBVGMSTREAM_SAMPLE_FLOAT   = 0x02,
};

typedef struct {
    /* main (always set) */
    const int channels;                     // output channels
    const int sample_rate;                  // output sample rate
    const int sample_type;                  // size of resulting samples

    /* extra info (may be 0 if not known or not relevant) */
    const uint32_t channel_layout;          // standard bitflags
    const int input_channels;               // original file's channels before downmixing (if any)

    const int interleave;                   // when file is interleaved
    const int frame_size;                   // when file has some configurable frame size

    const int subsong_index;                // 0 = none, N = loaded subsong N
    const int subsong_count;                // 0 = format has no subsongs, N = has N subsongs (1 = format has subsongs and only 1)

    /* sample info (may not be used depending on config) */
    const int64_t sample_count;             // file's max samples (not final play duration)
    const int64_t loop_start;               // loop start sample
    const int64_t loop_end;                 // loop end sample
    const bool loop_flag;                   // if file loops; note that false + defined loops means looping was forcefully disabled

    const int64_t play_time;				// samples after all calculations (after applying loop/fade/etc config)
                                            // ** may not be 100% accurate in some cases (must check decoder's 'done' flag)
                                            // ** if loop_forever is set this value is provided for reference based on non-forever config

    const bool rough_samples;               // signal cases where loop points or sample count can't exactly reflect actual behavior (do not use to export)

    const int stream_bitrate;               // average bitrate of the subsong (slightly bloated vs codec_bitrate: incorrect in rare cases)
    //const int codec_bitrate;              // average bitrate of the codec data (not possible/slow to calculate in most cases)

    const int format_internal_id;           // may be used when reopening subfiles or similar formats without checking other possible formats first
                                            // ** this value WILL change without warning between vgmstream versions/commits, do not store

    /* descriptions */
    const char codec[256];
    const char layout[256];
    const char metadata[256];

} libvgmstream_format_t;


typedef struct {
    void* buf;                              // current decoded buf (valid after _decode until next call; may change between calls)
    int buf_samples;                        // current buffer samples (0 is possible in some cases, meaning current _decode can't generate samples)
    int buf_bytes;                          // current buffer bytes (channels * sample-size * samples)

    bool done;                              // flag when stream is done playing based on config; will still allow _play calls returning blank samples
                                            // ** note that with play_forever this flag is never set
} libvgmstream_decoder_t;


/* vgmstream context/handle */
typedef struct {
    void* priv;                             // internal data

    /* pointers for easier ABI compatibility */
    libvgmstream_format_t* format;          // current song info, updated on _open
    libvgmstream_decoder_t* decoder;        // updated on each _decode call

} libvgmstream_t;



/* inits the vgmstream context
 * - returns NULL on error
 * - call libvgmstream_free when done.
 */
LIBVGMSTREAM_API libvgmstream_t* libvgmstream_init(void);

/* frees vgmstream context and any other internal stuff that may not be closed
 */
LIBVGMSTREAM_API void libvgmstream_free(libvgmstream_t* lib);


/* configures how vgmstream behaves internally when playing a file */
typedef struct {
    bool allow_play_forever;                // must allow manually as some cases a TXTP may set loop forever but client may not handle it (ex. wave dumpers)

    bool play_forever;                      // keeps looping forever (file must have loop_flag set)
    bool ignore_loop;                       // ignores loops points
    bool force_loop;                        // enables full loops (0..samples) if file doesn't have loop points
    bool really_force_loop;                 // forces full loops (0..samples) even if file has loop points
    bool ignore_fade;                       // don't fade after N loops and play remaning stream (for files with outros)

    double loop_count;                      // target loops (values like 1.5 are ok)
    double fade_delay;                      // fade delay after target loops
    double fade_time;                       // fade period after target loops

    int max_channels;                       // automatic downmixing if vgmstream's channels are higher than max_channels
                                            // ** for players that can only handle N channels, but this type of downmixing is very simplistic and not recommended

    int force_format;                       // forces output buffer to be remixed into some LIBVGMSTREAM_SAMPLE_x format

    //bool disable_config_override;         // ignore forced (TXTP) config

} libvgmstream_config_t;

/* pass default config, that will be applied to song on open
 * - invalid config or complex cases (ex. some TXTP) may ignore these settings.
 * - called without a song loaded (before _open or after _close), otherwise ignored.
 * - without config vgmstream will decode the current stream once
 */
LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg);


/* configures how vgmstream opens the format */
typedef struct {
    libvgmstream_streamfile_t* sf;          // custom IO streamfile that provides reader info for vgmstream
                                            // ** not needed after _open and should be closed, as vgmstream re-opens its own SFs internally as needed

    int subsong;                            // target subsong (1..N) or 0 = unknown/first
                                            // ** to check if a file has subsongs, _open first + check total_subsongs (then _open 2nd, 3rd, etc)

    void* external_buf;                     // set a custom-sized sample buf instead of the default provided buf 
                                            // ** must be at least as big as channels * sample-size * buf_samples
                                            // ** libvgmstream decoder->buf will set this buf
                                            // ** slower than using the provided buf (needs copying around), mainly if you have fixed sample constraints
    int external_buf_samples;               // max samples the custom buffer may hold

    int format_internal_id;                 // force a format (for example when loading new subsong)

    int stereo_track;                       // forces vgmstream to decode one 2ch+2ch+2ch... 'track' and discard other channels, where 0 = disabled, 1..N = Nth track

} libvgmstream_options_t;

/* opens file based on config and prepares it to play if supported.
 * - returns < 0 on error (file not recognised, invalid subsong index, etc)
 * - will close currently loaded song if needed
 */
LIBVGMSTREAM_API int libvgmstream_open(libvgmstream_t* lib, libvgmstream_options_t* open_options);

/* opens file based on config and returns its file info
 * - returns < 0 on error (file not recognised, invalid subsong index, etc)
 * - equivalent to _open, but doesn't update the current loaded song / format and copies to passed struct
 *   - may be used while current song is loaded/playing but you need to query next song with current config
 *   - to play a new song don't call _open_info to check the format first, just call _open + check format afterwards
 * - the only way for vgmstream to know a file's metadata is getting it almost ready to play,
 *   so this isn't any faster than _open
 */
LIBVGMSTREAM_API int libvgmstream_open_info(libvgmstream_t* lib, libvgmstream_options_t* open_options, libvgmstream_format_t* format);

/* closes current song; may still use libvgmstream to open other songs
 */
LIBVGMSTREAM_API void libvgmstream_close(libvgmstream_t* lib);


/* decodes next batch of samples (lib->decoder->* will be updated)
 * - returns < 0 on error
 */
LIBVGMSTREAM_API int libvgmstream_play(libvgmstream_t* lib);

/* Gets current position within the song.
 * - return < 0 on error (file not ready) */
LIBVGMSTREAM_API int64_t libvgmstream_play_position(libvgmstream_t* lib);

/* Seeks to absolute position. Will clamp incorrect values. 
 * - return < 0 on error (ignores seek attempt)
 * - on play_forever one may locally seek to any position, but there is an internal limit */
LIBVGMSTREAM_API int libvgmstream_seek(libvgmstream_t* lib, int64_t sample);


/* Writes a description of the format into dst. Will always be null-terminated.
 * - returns < 0 if file was truncated, though will still succeed. */
LIBVGMSTREAM_API int libvgmstream_format_describe(libvgmstream_format_t* format, char* dst, int dst_size);

typedef struct {
    bool is_extension;           /* set if filename is just an extension */
    bool skip_default;           /* set if shouldn't check default formats */
    bool reject_extensionless;   /* set if player can't play extensionless files */
    bool accept_unknown;         /* set to allow any extension (for txth) */
    bool accept_common;          /* set to allow known-but-common extension (when player has plugin priority) */
} libvgmstream_valid_t;

/* returns if vgmstream can parse a filename by extension, to reject some files earlier
 * - doesn't check file contents (that's only done on _open)
 * - config may be NULL
 * - mainly for plugins that fail early; libvgmstream doesn't use this
 */
LIBVGMSTREAM_API bool libvgmstream_is_valid(const char* filename, libvgmstream_valid_t* cfg);


//TODO are all these options necessary?
typedef struct {
    bool force_title;
    bool subsong_range;
    bool remove_extension;
    bool remove_archive;
    const char* filename;
} libvgmstream_title_t;

/* get a simple title for plugins, derived from internal stream name if available
 * - valid after _open */
LIBVGMSTREAM_API void libvgmstream_get_title(libvgmstream_t* lib, libvgmstream_title_t* cfg, char* buf, int buf_len);


enum {
    LIBVGMSTREAM_LOG_LEVEL_ALL      = 0,
    LIBVGMSTREAM_LOG_LEVEL_DEBUG    = 20,
    LIBVGMSTREAM_LOG_LEVEL_INFO     = 30,
    LIBVGMSTREAM_LOG_LEVEL_NONE     = 100,
};

typedef struct {
    int level;                                      // log level
    void (*callback)(int level, const char* str);   // log callback
    bool stdout_callback;                           // use log callback
} libvgmstream_log_t;

/* defines a global log callback, as vgmstream sometimes communicates format issues to the user.
 * - note that log is currently set globally rather than per libvgmstream_t
*/
LIBVGMSTREAM_API void libvgmstream_set_log(libvgmstream_log_t* log);

#endif
#endif
