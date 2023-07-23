/*
 * vgmstream.h - definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */
#ifndef _VGMSTREAM_H
#define _VGMSTREAM_H

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
enum { 
    /* Windows generally only allows 260 chars in path, but other OSs have higher limits, and we handle
     * UTF-8 (that typically uses 2-bytes for common non-latin codepages) plus player may append protocols
     * to paths, so it should be a bit higher. Most people wouldn't use huge paths though. */
    PATH_LIMIT = 4096, /* (256 * 8) * 2 = ~max_path * (other_os+extra) * codepage_bytes */
    STREAM_NAME_SIZE = 255,
    VGMSTREAM_MAX_CHANNELS = 64,
    VGMSTREAM_MIN_SAMPLE_RATE = 300, /* 300 is Wwise min */
    VGMSTREAM_MAX_SAMPLE_RATE = 192000, /* found in some FSB5 */
    VGMSTREAM_MAX_SUBSONGS = 65535, /* +20000 isn't that uncommon */
    VGMSTREAM_MAX_NUM_SAMPLES = 1000000000, /* no ~5h vgm hopefully */
};

#include "streamfile.h"
#include "vgmstream_types.h"
#include "util/log.h"

#ifdef VGM_USE_MP4V2
#define MP4V2_NO_STDINT_DEFS
#include <mp4v2/mp4v2.h>
#endif

#ifdef VGM_USE_FDKAAC
#include <aacdecoder_lib.h>
#endif

#include "coding/g72x_state.h"


typedef struct {
    int config_set; /* some of the mods below are set */

    /* modifiers */
    int play_forever;
    int ignore_loop;
    int force_loop;
    int really_force_loop;
    int ignore_fade;

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
    int pad_begin_set;
    int trim_begin_set;
    int body_time_set;
    int loop_count_set;
    int trim_end_set;
    int fade_delay_set;
    int fade_time_set;
    int pad_end_set;

    /* for lack of a better place... */
    int is_txtp;
    int is_mini_txtp;

} play_config_t;


typedef struct {
    int input_channels;
    int output_channels;

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


/* info for a single vgmstream channel */
typedef struct {
    STREAMFILE* streamfile;     /* file used by this channel */
    off_t channel_start_offset; /* where data for this channel begins */
    off_t offset;               /* current location in the file */

    off_t frame_header_offset;  /* offset of the current frame header (for WS) */
    int samples_left_in_frame;  /* for WS */

    /* format specific */

    /* adpcm */
    int16_t adpcm_coef[16];             /* formats with decode coefficients built in (DSP, some ADX) */
    int32_t adpcm_coef_3by32[0x60];     /* Level-5 0x555 */
    int16_t vadpcm_coefs[8*2*8];        /* VADPCM: max 8 groups * max 2 order * fixed 8 subframe coefs */
    union {
        int16_t adpcm_history1_16;      /* previous sample */
        int32_t adpcm_history1_32;
    };
    union {
        int16_t adpcm_history2_16;      /* previous previous sample */
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

    int adpcm_step_index;               /* for IMA */
    int adpcm_scale;                    /* for MS ADPCM */

    /* state for G.721 decoder, sort of big but we might as well keep it around */
    struct g72x_state g72x_state;

    /* ADX encryption */
    int adx_channels;
    uint16_t adx_xor;
    uint16_t adx_mult;
    uint16_t adx_add;

} VGMSTREAMCHANNEL;


/* main vgmstream info */
typedef struct {
    /* basic config */
    int32_t num_samples;            /* the actual max number of samples */
    int32_t sample_rate;            /* sample rate in Hz */
    int channels;                   /* number of channels */
    coding_t coding_type;           /* type of encoding */
    layout_t layout_type;           /* type of layout */
    meta_t meta_type;               /* type of metadata */

    /* loopin config */
    int loop_flag;                  /* is this stream looped? */
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
    int allow_dual_stereo;          /* search for dual stereo (file_L.ext + file_R.ext = single stereo file) */


    /* layout/block state */
    size_t full_block_size;         /* actual data size of an entire block (ie. may be fixed, include padding/headers, etc) */
    int32_t current_sample;         /* sample point within the file (for loop detection) */
    int32_t samples_into_block;     /* number of samples into the current block/interleave/segment/etc */
    off_t current_block_offset;     /* start of this block (offset of block header) */
    size_t current_block_size;      /* size in usable bytes of the block we're in now (used to calculate num_samples per block) */
    int32_t current_block_samples;  /* size in samples of the block we're in now (used over current_block_size if possible) */
    off_t next_block_offset;        /* offset of header of the next block */

    /* loop state (saved when loop is hit to restore later) */
    int32_t loop_current_sample;    /* saved from current_sample (same as loop_start_sample, but more state-like) */
    int32_t loop_samples_into_block;/* saved from samples_into_block */
    off_t loop_block_offset;        /* saved from current_block_offset */
    size_t loop_block_size;         /* saved from current_block_size */
    int32_t loop_block_samples;     /* saved from current_block_samples */
    off_t loop_next_block_offset;   /* saved from next_block_offset */
    int hit_loop;                   /* save config when loop is hit, but first time only */


    /* decoder config/state */
    int codec_endian;               /* little/big endian marker; name is left vague but usually means big endian */
    int codec_config;               /* flags for codecs or layouts with minor variations; meaning is up to them */
    int32_t ws_output_size;         /* WS ADPCM: output bytes for this block */


    /* main state */
    VGMSTREAMCHANNEL* ch;           /* array of channels */
    VGMSTREAMCHANNEL* start_ch;     /* shallow copy of channels as they were at the beginning of the stream (for resets) */
    VGMSTREAMCHANNEL* loop_ch;      /* shallow copy of channels as they were at the loop point (for loops) */
    void* start_vgmstream;          /* shallow copy of the VGMSTREAM as it was at the beginning of the stream (for resets) */

    void* mixing_data;              /* state for mixing effects */

    /* Optional data the codec needs for the whole stream. This is for codecs too
     * different from vgmstream's structure to be reasonably shoehorned.
     * Note also that support must be added for resetting, looping and
     * closing for every codec that uses this, as it will not be handled. */
    void* codec_data;
    /* Same, for special layouts. layout_data + codec_data may exist at the same time. */
    void* layout_data;


    /* play config/state */
    int config_enabled;             /* config can be used */
    play_config_t config;           /* player config (applied over decoding) */
    play_state_t pstate;            /* player state (applied over decoding) */
    int loop_count;                 /* counter of complete loops (1=looped once) */
    int loop_target;                /* max loops before continuing with the stream end (loops forever if not set) */
    sample_t* tmpbuf;               /* garbage buffer used for seeking/trimming */
    size_t tmpbuf_size;             /* for all channels (samples = tmpbuf_size / channels) */

} VGMSTREAM;


/* for files made of "continuous" segments, one per section of a song (using a complete sub-VGMSTREAM) */
typedef struct {
    int segment_count;
    VGMSTREAM** segments;
    int current_segment;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int mixed_channels;     /* segments have different number of channels */
} segmented_layout_data;

/* for files made of "parallel" layers, one per group of channels (using a complete sub-VGMSTREAM) */
typedef struct {
    int layer_count;
    VGMSTREAM** layers;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int external_looping;   /* don't loop using per-layer loops, but layout's own looping */
    int curr_layer;         /* helper */
} layered_layout_data;


#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
typedef struct {
    STREAMFILE* streamfile;
    uint64_t start;
    uint64_t offset;
    uint64_t size;
} mp4_streamfile;

typedef struct {
    mp4_streamfile if_file;
    MP4FileHandle h_mp4file;
    MP4TrackId track_id;
    unsigned long sampleId, numSamples;
    UINT codec_init_data_size;
    HANDLE_AACDECODER h_aacdecoder;
    unsigned int sample_ptr, samples_per_frame, samples_discard;
    INT_PCM sample_buffer[( (6) * (2048)*4 )];
} mp4_aac_codec_data;
#endif

// VGMStream description in structure format
typedef struct {
    int sample_rate;
    int channels;
    struct mixing_info {
        int input_channels;
        int output_channels;
    } mixing_info;
    int channel_layout;
    struct loop_info {
        int start;
        int end;
    } loop_info;
    size_t num_samples;
    char encoding[128];
    char layout[128];
    struct interleave_info {
        int value;
        int first_block;
        int last_block;
    } interleave_info;
    int frame_size;
    char metadata[128];
    int bitrate;
    struct stream_info {
        int current;
        int total;
        char name[128];
    } stream_info;
} vgmstream_info;

/* -------------------------------------------------------------------------*/
/* vgmstream "public" API                                                   */
/* -------------------------------------------------------------------------*/

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM* init_vgmstream(const char* const filename);

/* init with custom IO via streamfile */
VGMSTREAM* init_vgmstream_from_STREAMFILE(STREAMFILE* sf);

/* reset a VGMSTREAM to start of stream */
void reset_vgmstream(VGMSTREAM* vgmstream);

/* close an open vgmstream */
void close_vgmstream(VGMSTREAM* vgmstream);

/* calculate the number of samples to be played based on looping parameters */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM* vgmstream);

/* Decode data into sample buffer. Returns < sample_count on stream end */
int render_vgmstream(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);

/* Seek to sample position (next render starts from that point). Use only after config is set (vgmstream_apply_config) */
void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample);

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM* vgmstream, char* desc, int length);
void describe_vgmstream_info(VGMSTREAM* vgmstream, vgmstream_info* desc);

/* Return the average bitrate in bps of all unique files contained within this stream. */
int get_vgmstream_average_bitrate(VGMSTREAM* vgmstream);

/* List supported formats and return elements in the list, for plugins that need to know.
 * The list disables some common formats that may conflict (.wav, .ogg, etc). */
const char** vgmstream_get_formats(size_t* size);

/* same, but for common-but-disabled formats in the above list. */
const char** vgmstream_get_common_formats(size_t* size);

/* Force enable/disable internal looping. Should be done before playing anything (or after reset),
 * and not all codecs support arbitrary loop values ATM. */
void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample);

/* Set number of max loops to do, then play up to stream end (for songs with proper endings) */
void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target);

/* Return 1 if vgmstream detects from the filename that said file can be used even if doesn't physically exist */
int vgmstream_is_virtual_filename(const char* filename);

/* -------------------------------------------------------------------------*/
/* vgmstream "private" API                                                  */
/* -------------------------------------------------------------------------*/

/* Allocate initial memory for the VGMSTREAM */
VGMSTREAM* allocate_vgmstream(int channel_count, int looped);

/* Prepare the VGMSTREAM's initial state once parsed and ready, but before playing. */
void setup_vgmstream(VGMSTREAM* vgmstream);

/* Open the stream for reading at offset (taking into account layouts, channels and so on).
 * Returns 0 on failure */
int vgmstream_open_stream(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset);
int vgmstream_open_stream_bf(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset, int force_multibuffer);

/* Get description info */
void get_vgmstream_coding_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_layout_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_meta_description(VGMSTREAM* vgmstream, char* out, size_t out_size);

void setup_state_vgmstream(VGMSTREAM* vgmstream);
#endif
