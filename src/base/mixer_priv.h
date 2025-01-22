#ifndef _MIXER_PRIV_H_
#define _MIXER_PRIV_H_
#include "../streamtypes.h"
#include "mixer.h"
#include "sbuf.h"

#define VGMSTREAM_MAX_MIXING 512

typedef enum {
    MIX_SWAP,
    MIX_ADD,
    MIX_VOLUME,
    MIX_LIMIT,
    MIX_UPMIX,
    MIX_DOWNMIX,
    MIX_KILLMIX,
    MIX_FADE
} mix_type_t;

typedef struct {
    mix_type_t type;
    /* common */
    int ch_dst;
    int ch_src;
    float vol;

    /* fade envelope */
    float vol_start;    /* volume from pre to start */
    float vol_end;      /* volume from end to post */
    char shape;         /* curve type */
    int32_t time_pre;   /* position before time_start where vol_start applies (-1 = beginning) */
    int32_t time_start; /* fade start position where vol changes from vol_start to vol_end */
    int32_t time_end;   /* fade end position where vol changes from vol_start to vol_end */
    int32_t time_post;  /* position after time_end where vol_end applies (-1 = end) */
} mix_op_t;

struct mixer_t {
    int input_channels;     /* starting channels before mixing */
    int output_channels;    /* resulting channels after mixing */
    int mixing_channels;    /* max channels needed to mix */

    bool active;            /* mixing working */

    int chain_count;        /* op number */
    size_t chain_size;      /* max ops */
    mix_op_t chain[VGMSTREAM_MAX_MIXING]; /* effects to apply (could be alloc'ed but to simplify...) */

    /* fades only apply at some points, other mixes are active */
    bool has_non_fade;
    bool has_fade;

    float* mixbuf;          // internal mixing buffer
    sbuf_t smix;            // temp sbuf
    int32_t current_subpos; // state: current sample pos in the stream

    sfmt_t force_type;      // mixer output is original buffer's by default, unless forced
};

void mixer_op_swap(mixer_t* mixer, mix_op_t* op);
void mixer_op_add(mixer_t* mixer, mix_op_t* op);
void mixer_op_volume(mixer_t* mixer, mix_op_t* op);
void mixer_op_limit(mixer_t* mixer, mix_op_t* op);
void mixer_op_upmix(mixer_t* mixer, mix_op_t* op);
void mixer_op_downmix(mixer_t* mixer, mix_op_t* op);
void mixer_op_killmix(mixer_t* mixer, mix_op_t* op);
void mixer_op_fade(mixer_t* mixer, mix_op_t* op);
bool mixer_op_fade_is_active(mixer_t* mixer, int32_t current_start, int32_t current_end);
#endif
