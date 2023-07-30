#ifndef _MIXING_PRIV_H_
#define _MIXING_PRIV_H_

#include "../vgmstream.h"
#define VGMSTREAM_MAX_MIXING 512

/* mixing info */
typedef enum {
    MIX_SWAP,
    MIX_ADD,
    MIX_ADD_COPY,
    MIX_VOLUME,
    MIX_LIMIT,
    MIX_UPMIX,
    MIX_DOWNMIX,
    MIX_KILLMIX,
    MIX_FADE
} mix_command_t;

typedef struct {
    mix_command_t command;
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
} mix_command_data;

typedef struct {
    int mixing_channels;    /* max channels needed to mix */
    int output_channels;    /* resulting channels after mixing */
    int mixing_on;          /* mixing allowed */
    int mixing_count;       /* mixing number */
    size_t mixing_size;     /* mixing max */
    mix_command_data mixing_chain[VGMSTREAM_MAX_MIXING]; /* effects to apply (could be alloc'ed but to simplify...) */
    float* mixbuf;          /* internal mixing buffer */

    /* fades only apply at some points, other mixes are active */
    int has_non_fade;
    int has_fade;
} mixing_data;


#endif
