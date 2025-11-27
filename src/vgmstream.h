/*
 * vgmstream.h - internal definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */
#ifndef _VGMSTREAM_H_
#define _VGMSTREAM_H_

/* Due mostly to licensing issues, Vorbis, MPEG, G.722.1, etc decoding is done by external libraries.
 * Libs are disabled by default, defined on compile-time for builds that support it */
//#define VGM_USE_VORBIS
//#define VGM_USE_MPEG
//#define VGM_USE_G7221
//#define VGM_USE_G719
//#define VGM_USE_MP4V2
//#define VGM_USE_FDKAAC
//#define VGM_USE_FFMPEG
//#define VGM_USE_ATRAC9
//#define VGM_USE_CELT
//#define VGM_USE_SPEEX


/* reasonable limits */
#include "util/vgmstream_limits.h"

#include "streamfile.h"
#include "vgmstream_types.h"

#include "coding/g72x_state.h"


typedef struct {
    bool config_set; /* some of the mods below are set */

    /* modifiers */
    bool play_forever;
    bool ignore_loop;
    bool force_loop;
    bool really_force_loop;
    bool ignore_fade;

    /* processing */
    double loop_count;
    int32_t pad_begin;
    int32_t trim_begin;
    int32_t body_time;
    int32_t trim_end;
    double fade_delay; /* not in samples for backwards compatibility */
    double fade_time;
    int32_t pad_end;

    double pad_begin_s;
    double trim_begin_s;
    double body_time_s;
    double trim_end_s;
  //double fade_delay_s;
  //double fade_time_s;
    double pad_end_s;

    /* internal flags */
    bool pad_begin_set;
    bool trim_begin_set;
    bool body_time_set;
    bool loop_count_set;
    bool trim_end_set;
    bool fade_delay_set;
    bool fade_time_set;
    bool pad_end_set;

    /* for lack of a better place... */
    bool is_txtp;
    bool is_mini_txtp;

} play_config_t;


typedef struct {
    int32_t pad_begin_duration;
    int32_t pad_begin_left;
    int32_t trim_begin_duration;
    int32_t trim_begin_left;
    int32_t body_duration;
    int32_t fade_duration;
    int32_t fade_left;
    int32_t fade_start;
    int32_t pad_end_duration;
  //int32_t pad_end_left;
    int32_t pad_end_start;

    int32_t play_duration;      /* total samples that the stream lasts (after applying all config) */
    int32_t play_position;      /* absolute sample where stream is */

} play_state_t;


/* info for a single vgmstream 'channel' (or rather, mono stream) */
typedef struct {
    STREAMFILE* streamfile;     /* file used by this channel */
    off_t channel_start_offset; /* where data for this channel begins */
    off_t offset;               /* current location in the file */

    /* format and channel specific */

    /* ADPCM with built or variable decode coefficients */
    union {
        int16_t adpcm_coef[16];         /* DSP, some ADX (in rare cases may change per block) */
        int16_t vadpcm_coefs[8*2*8];    /* VADPCM: max 8 groups * max 2 order * fixed 8 subframe = 128 coefs */
        int32_t adpcm_coef_3by32[96];   /* Level-5 0x555 */
    };

    /* previous ADPCM samples */
    union {
        int16_t adpcm_history1_16;
        int32_t adpcm_history1_32;
    };
    union {
        int16_t adpcm_history2_16;
        int32_t adpcm_history2_32;
    };
    union {
        int16_t adpcm_history3_16;
        int32_t adpcm_history3_32;
    };
    union {
        int16_t adpcm_history4_16;
        int32_t adpcm_history4_32;
    };

    //double adpcm_history1_double;
    //double adpcm_history2_double;

    /* for ADPCM decoders that store steps (IMA) or scales (MSADPCM) */
    union {
        int adpcm_step_index;
        int adpcm_scale;
    };

    /* Westwood Studios decoder */
    off_t ws_frame_header_offset;       /* offset of the current frame header */
    int ws_samples_left_in_frame;       /* last decoded info */

    /* state for G.721 decoder, sort of big but we might as well keep it around */
    struct g72x_state g72x_state;

    /* ADX encryption */
    uint16_t adx_xor;
    uint16_t adx_mult;
    uint16_t adx_add;

} VGMSTREAMCHANNEL;


/* main vgmstream info */
typedef struct {
    /* basic config */
    int channels;                   /* number of channels for the current stream */
    int32_t sample_rate;            /* sample rate in Hz */
    int32_t num_samples;            /* the actual max number of samples */
    coding_t coding_type;           /* type of encoding */
    layout_t layout_type;           /* type of layout */
    meta_t meta_type;               /* type of metadata */

    /* loop config */
    bool loop_flag;                 /* is this stream looped? */
    int32_t loop_start_sample;      /* first sample of the loop (included in the loop) */
    int32_t loop_end_sample;        /* last sample of the loop (not included in the loop) */

    /* layouts/block config */
    size_t interleave_block_size;   /* interleave, or block/frame size (depending on the codec) */
    size_t interleave_first_block_size; /* different interleave for first block */
    size_t interleave_first_skip;   /* data skipped before interleave first (needed to skip other channels) */
    size_t interleave_last_block_size; /* smaller interleave for last block */
    size_t frame_size;              /* for codecs with configurable size */

    /* subsong config */
    int num_streams;                /* for multi-stream formats (0=not set/one stream, 1=one stream) */
    int stream_index;               /* selected subsong (also 1-based) */
    size_t stream_size;             /* info to properly calculate bitrate in case of subsongs */
    char stream_name[STREAM_NAME_SIZE]; /* name of the current stream (info), if the file stores it and it's filled */

    /* mapping config (info for plugins) see channel_mappings.h */
    uint32_t channel_layout;        /* order: FL FR FC LFE BL BR FLC FRC BC SL SR etc (WAVEFORMATEX flags where FL=lowest bit set) */

    /* other config */
    bool allow_dual_stereo;         /* search for dual stereo (file_L.ext + file_R.ext = single stereo file) */
    int format_id;                  /* internal format ID */


    /* decoder config/state */
    int codec_endian;               /* little/big endian marker; name is left vague but usually means big endian */
    int codec_config;               /* flags for codecs or layouts with minor variations; meaning is up to them (may change during decode) */
    bool codec_internal_updates;    /* temp(?) kludge (see vgmstream_open_stream/decode) */
    int32_t ws_output_size;         /* WS ADPCM: output bytes for this block */

    /* layout/block state */
    int32_t current_sample;         /* sample point within the stream (for loop detection) */
    int32_t samples_into_block;     /* number of samples into the current block/interleave/segment/etc */
    off_t current_block_offset;     /* start of this block (offset of block header) */
    size_t current_block_size;      /* size in usable bytes of the block we're in now (used to calculate num_samples per block) */
    int32_t current_block_samples;  /* size in samples of the block we're in now (used over current_block_size if possible) */
    off_t next_block_offset;        /* offset of header of the next block */
    size_t full_block_size;         /* actual data size of an entire block (ie. may be fixed, include padding/headers, etc) */

    /* layout/block state copy for loops (saved on loop_start and restored later on loop_end) */
    int32_t loop_current_sample;    /* saved from current_sample (same as loop_start_sample, but more state-like) */
    int32_t loop_samples_into_block;/* saved from samples_into_block */
    off_t loop_block_offset;        /* saved from current_block_offset */
    size_t loop_block_size;         /* saved from current_block_size */
    int32_t loop_block_samples;     /* saved from current_block_samples */
    off_t loop_next_block_offset;   /* saved from next_block_offset */
    size_t loop_full_block_size;    /* saved from full_block_size (probably unnecessary) */

    bool hit_loop;                  /* save config when loop is hit, but first time only */

    /* main state */
    VGMSTREAMCHANNEL* ch;           /* array of channels with current offset + per-channel codec config */

    VGMSTREAMCHANNEL* loop_ch;      /* shallow copy of channels as they were at the loop point (for loops) */

    void* start_vgmstream;          /* shallow copy of the VGMSTREAM as it was at the beginning of the stream (for resets) */
    VGMSTREAMCHANNEL* start_ch;     /* shallow copy of channels as they were at the beginning of the stream (for resets) */

    void* mixer;                    /* state for mixing effects */

    /* Optional data the codec needs for the whole stream. This is for codecs too
     * different from vgmstream's structure to be reasonably shoehorned.
     * Note also that support must be added for resetting, looping and
     * closing for every codec that uses this, as it will not be handled. */
    void* codec_data;
    /* Same, for special layouts. layout_data + codec_data may exist at the same time. */
    void* layout_data;


    /* play config/state */
    bool config_enabled;            /* config can be used */
    play_config_t config;           /* player config (applied over decoding) */
    play_state_t pstate;            /* player state (applied over decoding) */
    int loop_count;                 /* counter of complete loops (1=looped once) */
    int loop_target;                /* max loops before continuing with the stream end (loops forever if not set) */

    void* tmpbuf;                   /* garbage buffer used for seeking/trimming */
    size_t tmpbuf_size;             /* for all channels (samples = tmpbuf_size / channels / sample_size) */

    void* decode_state;             /* for some decoders (TO-DO: to be moved around) */
    void* seek_table;               /* for some decoders (TO-DO: to be moved around) */
} VGMSTREAM;


/* -------------------------------------------------------------------------*/
/* vgmstream internal API                                                   */
/* -------------------------------------------------------------------------*/

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM* init_vgmstream(const char* const filename);

/* init with custom IO via streamfile */
VGMSTREAM* init_vgmstream_from_STREAMFILE(STREAMFILE* sf);

/* reset a VGMSTREAM to start of stream */
void reset_vgmstream(VGMSTREAM* vgmstream);

/* close an open vgmstream */
void close_vgmstream(VGMSTREAM* vgmstream);

// If you are using vgmstream as a library and wonder what happened to render_vgmstream(), you can use
// render_vgmstream2() for the time being, it's 100% the same (it was renamed to bring this into attention,
// typedef as needed). However, please migrate to the public API (see libvgmstream.h) instead, it's
// fairly straightforward (see "Use external API" and related commits for various plugins).
// In next versions the "API" in this .h will be removed without warning as it wasn't really intended to be
// used by external projects. I hate breaking things but it's becoming very hard to improve anything otherwise, sorry.
/* Decode data into sample buffer. Returns < sample_count on stream end */
int render_vgmstream2(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);

/* Seek to sample position (next render starts from that point). Use only after config is set (vgmstream_apply_config) */
void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample);

/* -------------------------------------------------------------------------*/
/* vgmstream internal helpers                                               */
/* -------------------------------------------------------------------------*/

/* Allocate initial memory for the VGMSTREAM */
VGMSTREAM* allocate_vgmstream(int channel_count, int looped);

/* Prepare the VGMSTREAM's initial state once parsed and ready, but before playing. */
void setup_vgmstream(VGMSTREAM* vgmstream);

/* Open the stream for reading at offset (taking into account layouts, channels and so on). */
bool vgmstream_open_stream(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset);
bool vgmstream_open_stream_bf(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset, bool force_multibuffer);

/* Force enable/disable internal looping. Should be done before playing anything (or after reset),
 * and not all codecs support arbitrary loop values ATM. */
void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample);

/* Set number of max loops to do, then play up to stream end (for songs with proper endings) */
void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target);

void setup_vgmstream_play_state(VGMSTREAM* vgmstream);

/* Return 1 if vgmstream detects from the filename that said file can be used even if doesn't physically exist */
bool vgmstream_is_virtual_filename(const char* filename);

#endif
