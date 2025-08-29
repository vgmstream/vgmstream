#ifndef _META_UTILS_H
#define _META_UTILS_H

#include "../streamtypes.h"
#include "reader_get.h"
#include "reader_put.h"
#include "../coding/coding.h"


/* Helper struct for common numbers (no need to use all), to use with helper functions.
 * Preferably declare after validating header ID as it's faster (by a minuscule amount). */
typedef struct {
    /* should be set */
    int channels;
    int sample_rate;
    int32_t num_samples;

    /* optional info */
    bool loop_flag;
    int32_t loop_start;
    int32_t loop_end;

    int target_subsong;
    int total_subsongs;

    int32_t interleave;
    int32_t interleave_last;

    /* common helpers */
    uint32_t stream_offset; /* where current stream starts */
    uint32_t stream_size;   /* current stream size */
    uint32_t data_offset;   /* where data (first stream) starts */
    uint32_t data_size;     /* data for all streams */
    uint32_t head_size;     /* size of some header part */
    uint32_t chan_offset;
    uint32_t chan_size;

    uint32_t coefs_offset;
    uint32_t coefs_spacing;
    uint32_t hists_offset;
    uint32_t hists_spacing;

    uint32_t name_offset;

    /* optional but can be used for some actions (such as DSP coefs) */
    bool big_endian;
    coding_t coding;
    layout_t layout;
    meta_t meta;

    /* only sf_head is used to read coefs and such */
    STREAMFILE* sf;
    STREAMFILE* sf_head;
    STREAMFILE* sf_body;

    bool open_stream;

    bool has_subsongs;
    bool has_empty_banks;
    bool allow_dual_stereo;
} meta_header_t;

VGMSTREAM* alloc_metastream(meta_header_t* h);

/* checks max subsongs and setups target */
//bool check_subsongs(int* target_subsong, int total_subsongs);

/* marks a stream name as missing (for missing entries) */
void meta_mark_missing(VGMSTREAM* v);

//void meta_mark_dummy(VGMSTREAM* v);


#endif
