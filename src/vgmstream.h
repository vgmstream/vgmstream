/*
 * vgmstream.h - definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */

#ifndef _VGMSTREAM_H
#define _VGMSTREAM_H

#include "streamfile.h"
#include "coding/g72x_state.h"

/* The encoding type specifies the format the sound data itself takes */
typedef enum {
    /* 16-bit PCM */
    coding_PCM16BE,         /* big endian 16-bit PCM */
    coding_PCM16LE,         /* little endian 16-bit PCM */
    /* 8-bit PCM */
    coding_PCM8,            /* 8-bit PCM */
    /* 4-bit ADPCM */
    coding_NDS_IMA,         /* IMA ADPCM w/ NDS layout */
    coding_CRI_ADX,         /* CRI ADX */
    coding_NGC_DSP,         /* NGC ADPCM, called DSP */
    coding_NGC_DTK,         /* NGC hardware disc ADPCM, called DTK, TRK or ADP */
    coding_G721,            /* CCITT G.721 ADPCM */
    coding_NGC_AFC,         /* NGC ADPCM, called AFC */
	coding_PSX,				/* PSX & PS2 ADPCM */
	coding_XA,				/* PSX CD-XA */
} coding_t;

/* The layout type specifies how the sound data is laid out in the file */
typedef enum {
    /* generic */
    layout_none,            /* straight data */
    /* interleave */
    layout_interleave,      /* equal interleave throughout the stream */
    layout_interleave_shortblock, /* interleave with a short last block */
#if 0
    layout_interleave_byte,  /* full byte interleave */
#endif
    /* headered blocks */
    layout_ast_blocked,     /* .ast STRM with BLCK blocks*/
    layout_halpst_blocked,    /* blocks with HALPST-format header */
	layout_xa_blocked,
#if 0
    layout_strm_blocked,    /* */
#endif
    /* otherwise odd */
    layout_dtk_interleave,  /* dtk interleaves channels by nibble */
} layout_t;

/* The meta type specifies how we know what we know about the file. We may know because of a header we read, some of it may have been guessed from filenames, etc. */
typedef enum {
    /* DSP-specific */
    meta_DSP_STD,           /* standard GC ADPCM (DSP) header */
    meta_DSP_CSTR,          /* Star Fox Assault "Cstr" */
    meta_DSP_RS03,          /* Metroid Prime 2 "RS03" */
    meta_DSP_STM,           /* Paper Mario 2 STM */
    meta_DSP_HALP,          /* SSB:M "HALPST" */
    meta_DSP_AGSC,          /* Metroid Prime 2 title */
    meta_DSP_MPDSP,         /* Monopoly Party single header stereo */
    meta_DSP_JETTERS,       /* Bomberman Jetters .dsp */
    meta_DSP_MSS,
    meta_DSP_GCM,
    /* Nintendo */
    meta_STRM,              /* STRM */
    meta_RSTM,              /* RSTM (similar to STRM) */
    meta_AFC,               /* AFC */
    meta_AST,               /* AST */
    meta_RWSD,              /* single-stream RWSD */
    meta_RSTM_SPM,          /* RSTM with 44->22khz hack */
    /* CRI ADX */
    meta_ADX_03,            /* ADX "type 03" */
    meta_ADX_04,            /* ADX "type 04" */
	meta_ADX_05,            /* ADX "type 05" */

    /* etc */
    meta_NGC_ADPDTK,        /* NGC DTK/ADP, no header (.adp) */
    meta_kRAW,              /* almost headerless PCM */
    meta_RSF,               /* Retro Studios RSF, no header (.rsf) */
    meta_HALPST,            /* HAL Labs HALPST */
    meta_GCSW,              /* GCSW (PCM) */

	meta_PS2_SShd,			/* .ADS with SShd header */
	meta_PS2_NPSF,			/* Namco Production Sound File */
	meta_PS2_RXW,			/* Sony Arc The Lad Sound File */
	meta_PS2_RAW,			/* RAW Interleaved Format */
	meta_PS2_EXST,			/* Shadow of Colossus EXST */
	meta_PS2_SVAG,			/* Konami SVAG */
	meta_PS2_MIB,			/* MIB File */
	meta_PS2_MIB_MIH,		/* MIB File + MIH Header*/
	meta_PS2_MIC,			/* KOEI MIC File */

	meta_PSX_XA,			/* CD-XA with RIFF header */

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

    int adpcm_step_index;     /* for IMA */

    struct g72x_state g72x_state; /* state for G.721 decoder, sort of big but we
                               might as well keep it around */

#ifdef DEBUG
    int samples_done;
    int16_t loop_history1,loop_history2;
#endif
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
    VGMSTREAMCHANNEL * start_ch;    /* copies of channel status as they were at the beginning of the stream */
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

	uint8_t xa_channel;				/* Selected XA Channel */
	int32_t xa_sector_length;		/* XA block */
} VGMSTREAM;

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM * init_vgmstream(const char * const filename);

/* internal vgmstream that takes parameters the library user shouldn't have to know
 * about */
VGMSTREAM * init_vgmstream_internal(const char * const filename, int do_dfs);

/* allocate a VGMSTREAM and channel stuff */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped);

/* deallocate, close, etc. */
void close_vgmstream(VGMSTREAM * vgmstream);

/* calculate the number of samples to be played based on looping parameters */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM * vgmstream);

/* render! */
void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

/* smallest self-contained group of samples is a frame */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream);
/* number of bytes per frame */
int get_vgmstream_frame_size(VGMSTREAM * vgmstream);
/* in NDS IMA the frame size is the block size, so the last one is short */
int get_vgmstream_samples_per_shortframe(VGMSTREAM * vgmstream);
int get_vgmstream_shortframe_size(VGMSTREAM * vgmstream);

/* Assume that we have written samples_written into the buffer already, and we have samples_to_do consecutive
 * samples ahead of us. Decode those samples into the buffer. */
void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer);

/* calculate number of consecutive samples to do (taking into account stopping for loop start and end)  */
int vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM * vgmstream);

/* Detect start and save values, also detect end and restore values. Only works on exact sample values.
 * Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM * vgmstream);

/* Write a description of the stream into array pointed by desc,
 * which must be length bytes long. Will always be null-terminated if length > 0
 */
void describe_vgmstream(VGMSTREAM * vgmstream, char * desc, int length);

/* See if there is a second file which may be the second channel, given
 * already opened mono opened_stream which was opened from filename.
 * If a suitable file is found, open it and change opened_stream to a
 * stereo stream. */
void try_dual_file_stereo(VGMSTREAM * opened_stream, const char * const filename);

#endif
