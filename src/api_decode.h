#ifndef _API_DECODE_H_
#define _API_DECODE_H_
#include "api.h"
#if LIBVGMSTREAM_ENABLE
#include "api_streamfile.h"


/* vgmstream's main (decode) API.
 */


/* interleaved samples: buf[0]=ch0, buf[1]=ch1, buf[2]=ch0, buf[3]=ch0, ... */
typedef enum { 
    LIBVGMSTREAM_SAMPLE_PCM16   = 0x01,
    LIBVGMSTREAM_SAMPLE_PCM24   = 0x02,
    LIBVGMSTREAM_SAMPLE_PCM32   = 0x03,
    LIBVGMSTREAM_SAMPLE_FLOAT   = 0x04,
} libvgmstream_sample_t;

/* current song info, may be copied around (values are info-only) */
typedef struct {
    /* main (always set) */
    libvgmstream_sample_t sample_type;  // output buffer's sample type
    int channels;                       // output channels
    int sample_rate;                    // output sample rate

    int sample_size;                    // derived from sample_type (pcm16=2, float=4, etc)

    /* extra info (may be 0 if not known or not relevant) */
    uint32_t channel_layout;            // standard WAVE bitflags

    int subsong_index;                  // 0 = none, N = loaded subsong N
    int subsong_count;                  // 0 = format has no concept of subsongs, N = has N subsongs (1 = format has subsongs, and only 1)

    int input_channels;                 // original file's channels before downmixing (if any)
    //int interleave;                   // when file is interleaved
    //int interleave_first;             // when file is interleaved
    //int interleave_last;              // when file is interleaved
    //int frame_size;                   // when file has some configurable frame size

    /* sample info (may not be used depending on config) */
    int64_t sample_count;               // file's max samples (not final play duration)
    int64_t loop_start;                 // loop start sample
    int64_t loop_end;                   // loop end sample
    bool loop_flag;                     // if file loops; note that false + defined loops means looping was forcefully disabled

    bool play_forever;                  // if file loops forever based on current config (meaning _play never stops)
    int64_t play_samples;               // totals after all calculations (after applying loop/fade/etc config)
                                        // ** may not be 100% accurate in some cases (must check decoder's 'done' flag rather than this count)
                                        // ** if play_forever is set this is still provided for reference based on non-forever config

    int stream_bitrate;                 // average bitrate of the subsong (slightly bloated vs codec_bitrate; incorrect in rare cases)
    //int codec_bitrate;                // average bitrate of the codec data
                                        // ** not possible / slow to calculate in most cases

    /* descriptions */
    char codec_name[128];               //
    char layout_name[128];              //
    char meta_name[128];                // (not internal "tag" metadata)
    char stream_name[256];              // some internal name or representation, not always useful
    // ** these are a bit big for a struct, but the typical use case of vgsmtream is opening a file > immediately
    //    query description and since libvgmstream returns its own copy it shouldn't be too much of a problem
    // ** (may be separated later)

    /* misc */
    //bool rough_samples;               // signal cases where loop points or sample count can't exactly reflect actual behavior (do not use to export)

    int format_internal_id;             // may be used when reopening subfiles or similar formats without checking other possible formats first
                                        // ** this value WILL change without warning between vgmstream versions/commits

} libvgmstream_format_t;


typedef struct {
    void* buf;                              // current decoded buf (valid after _decode until next call; may change between calls)
    int buf_samples;                        // current buffer samples (0 is possible in some cases, meaning current _decode can't generate samples)
    int buf_bytes;                          // current buffer bytes (channels * sample_size * samples)

    bool done;                              // flag when stream is done playing based on config; will still allow _play calls returning blank samples
                                            // ** note that with play_forever this flag is never set
} libvgmstream_decoder_t;


/* vgmstream context/handle */
typedef struct {
    void* priv;                             // internal data

    /* pointers for easier ABI compatibility */
    const libvgmstream_format_t* format;    // current song info, updated on _open
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
    bool disable_config_override;           // ignore forced (TXTP) config
    bool allow_play_forever;                // must allow manually as some cases a TXTP may set loop forever but client may not handle it (ex. wave dumpers)

    bool play_forever;                      // keeps looping forever (file must have loop_flag set)
    bool ignore_loop;                       // ignores loops points
    bool force_loop;                        // enables full loops (0..samples) if file doesn't have loop points
    bool really_force_loop;                 // forces full loops (0..samples) even if file has loop points
    bool ignore_fade;                       // don't fade after N loops and play remaning stream (for files with outros)

    double loop_count;                      // target loops (values like 1.5 are ok)
    double fade_time;                       // fade period after target loops
    double fade_delay;                      // fade delay after target loops

    int auto_downmix_channels;              // downmixing if vgmstream's channels are higher than value
                                            // ** for players that can only handle N channels, but this type of downmixing is very simplistic and not recommended

    bool force_pcm16;                       // forces output buffer to be remixed into PCM16

} libvgmstream_config_t;

/* pass default config, that will be applied to song on open
 * - invalid config or complex cases (ex. some TXTP) may ignore these settings.
 * - called without a song loaded (before _open or after _close), otherwise ignored.
 * - without config vgmstream will decode the current stream once
 */
LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg);


/* configures how vgmstream opens the format */
typedef struct {
    libvgmstream_streamfile_t* libsf;       // custom IO streamfile that provides reader info for vgmstream
                                            // ** not needed after _open and should be closed, as vgmstream re-opens its own SFs internally as needed

    int subsong;                            // target subsong (1..N) or 0 = default/first
                                            // ** to check if a file has subsongs, _open first + check format->total_subsongs (then _open 2nd, 3rd, etc)

    int format_internal_id;                 // force a format (for example when loading new subsong of the same archive)

    int stereo_track;                       // forces vgmstream to decode one 2ch+2ch+2ch... 'track' and discard other channels, where 0 = disabled, 1..N = Nth track

} libvgmstream_options_t;

/* opens file based on config and prepares it to play if supported.
 * - returns < 0 on error (file not recognised, invalid subsong index, etc)
 * - will close currently loaded song if needed
 */
LIBVGMSTREAM_API int libvgmstream_open(libvgmstream_t* lib, libvgmstream_options_t* open_options);

#if 0
/* opens file based on config and returns its file info
 * - returns < 0 on error (file not recognised, invalid subsong index, etc)
 * - equivalent to _open, but doesn't update the current loaded song / format and copies to passed struct
 *   - may be used while current song is loaded/playing but you need to query next song with current config
 *   - to play a new song don't call _open_info to check the format first, just call _open + check format afterwards
 * - in many cases the only way for vgmstream to get a file's format info is making it almost ready to play,
 *   so this isn't any faster than _open
 */
LIBVGMSTREAM_API int libvgmstream_open_info(libvgmstream_t* lib, libvgmstream_options_t* options, libvgmstream_format_t* format);
#endif

/* closes current song; may still use libvgmstream to open other songs
 */
LIBVGMSTREAM_API void libvgmstream_close(libvgmstream_t* lib);


/* decodes next batch of samples
 * - vgmstream supplies its own buffer, updated on lib->decoder->* values (may change between calls)
 * - returns < 0 on error
 */
LIBVGMSTREAM_API int libvgmstream_play(libvgmstream_t* lib);

/* Same as _play, but fills some external buffer (also updates lib->decoder->* values)
 * - returns < 0 on error, or N = number of filled samples.
 * - buf must be at least as big as channels * sample_size * buf_samples
 * - needs copying around from internal bufs so may be slightly slower; mainly for cases when you have buf constraints
 */
LIBVGMSTREAM_API int libvgmstream_fill(libvgmstream_t* lib, void* buf, int buf_samples);

/* Gets current position within the song.
 * - return < 0 on error (file not ready)
 */
LIBVGMSTREAM_API int64_t libvgmstream_get_play_position(libvgmstream_t* lib);

/* Seeks to absolute position. Will clamp incorrect values such as seeking before/past playable length.
 * - on play_forever may seek to any position
 */
LIBVGMSTREAM_API void libvgmstream_seek(libvgmstream_t* lib, int64_t sample);

/* Reset current song
 */
LIBVGMSTREAM_API void libvgmstream_reset(libvgmstream_t* lib);

#endif
#endif
