/*
 * vgmstream.h - definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */

#include <inttypes.h>
#include "streamfile.h"

#ifndef _VGMSTREAM_H
#define _VGMSTREAM_H

/* The encoding type specifies the format the sound data itself takes */
typedef enum {
    /* 16-bit PCM */
    coding_PCM16BE,         /* big endian 16-bit PCM */
    coding_PCM16LE,         /* little endian 16-bit PCM */
    /* 8-bit PCM */
    coding_PCM8,            /* 8-bit PCM */
    /* 4-bit ADPCM */
    coding_NDS_IMA,         /* IMA ADPCM w/ NDS layout */
    coding_XBOX_IMA,        /* IMA ADPCM w/ XBOX layout */
    coding_CRI_ADX,         /* CRI ADX */
    coding_NGC_DSP,         /* NGC ADPCM, called DSP */
    coding_NGC_AFC,         /* other NGC ADPCM, called AFC */
    coding_NGC_DTK,         /* NGC hardware disc ADPCM, called DTK or ADP */
    coding_PS2ADPCM,        /* PS2 ADPCM, sometimes called "VAG" */
    coding_EA_XA,           /* Electronic Arts XA */
    coding_CD_XA,           /* CD-XA */
    coding_RSF,             /* Retro Studios RSF */
} coding_t;

/* The layout type specifies how the sound data is laid out in the file */
typedef enum {
    /* generic */
    layout_none,            /* straight data */
    /* interleave */
    layout_interleave,      /* equal interleave throughout the stream */
    layout_interleave_shortblock, /* interleave with a short last block */
    layout_interleave_byte,  /* full byte interleave */
    /* headered blocks */
    layout_halp_blocked,    /* blocks with HALP-format header */
    layout_ast_blocked,     /* */
    layout_strm_blocked,    /* */
} layout_t;

/* The meta type specifies how we know what we know about the file. We may know because of a header we read, some of it may have been guessed from filenames, etc. */
typedef enum {
    /* DSP-specific */
    meta_DSP_STD,           /* standard GC ADPCM (DSP) header */
    meta_DSP_CSTR,          /* Star Fox Assault "Cstr" */
    meta_DSP_RS03,          /* Metroid Prime 2 "RS03" */
    meta_DSP_STM,           /* Paper Mario 2 STM */
    meta_DSP_HALP,          /* SSB:M "HALPST" */
    /* Nintendo */
    meta_AST,               /* AST */
    meta_STRM,              /* STRM */
    meta_RSTM,              /* RSTM (similar to STRM) */
    /* CRI ADX */
    meta_ADX_03,            /* ADX "type 03" */
    meta_ADX_04,            /* ADX "type 04" */
    /* etc */
    meta_NGC_DTK,           /* NGC DTK/ADP */
    meta_kRAW,              /* almost headerless PCM */

} meta_t;

typedef struct {
    STREAMFILE * streamfile; /* file used by this channel */
    off_t channel_start_offset; /* where data for this channel begins */
    off_t offset;           /* current location in the file */

    /* format specific */

    /* adpcm */
    int16_t adpcm_coef[16]; /* for formats with decode coefficients built in */
    union {
        int16_t adpcm_history1_16;  /* previous sample */
        int32_t adpcm_history1_32;
    };
    union {
        int16_t adpcm_history2_16;  /* previous previous sample */
        int32_t adpcm_history2_32;
    };
} VGMSTREAMCHANNEL;

typedef struct {
    /* basics */
    int32_t num_samples;    /* the actual number of samples in this stream */
    int32_t sample_rate;    /* sample rate in Hz */
    int channels;           /* number of channels */
    coding_t coding_type;   /* type of encoding */
    layout_t layout_type;   /* type of layout for data */
    meta_t meta_type;       /* how we know the metadata */

    /* looping */
    int loop_flag;          /* is this stream looped? */
    int32_t loop_start_sample; /* first sample of the loop (included in the loop) */
    int32_t loop_end_sample; /* last sample of the loop (not included in the loop) */

    /* channels */
    VGMSTREAMCHANNEL * ch;   /* pointer to array of channels */

    /* channel copies
     * NOTE: Care must be taken when deallocating that the same STREAMFILE
     * isn't closed twice, but also that everything is deallocated. Generally
     * a channel should only have one STREAMFILE in its lifetime.
     */
    VGMSTREAMCHANNEL * start_ch;    /* coptes of channel status as they were at the beginning of the stream */
    VGMSTREAMCHANNEL * loop_ch;     /* copies of channel status as they were at the loop point */

    /* layout-specific */
    int32_t current_sample;         /* number of samples we've passed */
    int32_t samples_into_block;     /* number of samples into the current block */
    /* interleave */
    size_t interleave_block_size;   /* interleave for this file */
    size_t interleave_smallblock_size;  /* smaller interleave for last block */
    /* headered blocks */
    off_t start_block_offset;       /* first block in the file */
    off_t current_block_offset;     /* start of this block (offset of block header) */
    size_t current_block_size;      /* size of the block we're in now */
    off_t next_block_offset;        /* offset of header of the next block */

    int hit_loop;                   /* have we seen the loop yet? */

    /* loop layout (saved values) */
    int32_t loop_sample;            /* saved from current_sample, should be loop_start_sample... */
    int32_t loop_samples_into_block;    /* saved from samples_into_block */
    off_t loop_block_offset;        /* saved from current_block_offset */
    size_t loop_block_size;         /* saved from current_block_size */
    off_t loop_next_block_offset;   /* saved from next_block_offset */

} VGMSTREAM;

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM * init_vgmstream(const char * const filename);

/* allocate a VGMSTREAM and channel stuff */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped);

/* deallocate, close, etc. */
void close_vgmstream(VGMSTREAM * vgmstream);

/* calculate the number of samples to be played based on looping parameters */
int32_t get_vgmstream_play_samples(double looptimes, double fadetime, VGMSTREAM * vgmstream);

/* render! */
void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

/* smallest self-contained group of samples is a frame */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream);
/* number of bytes per frame */
int get_vgmstream_frame_size(VGMSTREAM * vgmstream);

/* Assume that we have written samples_written into the buffer already, and we have samples_to_do consecutive
 * samples ahead of us. Decode those samples into the buffer. */
void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer);

/* calculate number of consecutive samples to do (taking into account stopping for loop start and end)  */
int vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM * vgmstream);

/* Detect start and save values, also detect end and restore values. Only works on exact sample values.
 * Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM * vgmstream);

#endif
