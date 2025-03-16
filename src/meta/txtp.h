#ifndef _TXTP_H_
#define _TXTP_H_

#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../base/mixing.h"
#include "../base/plugins.h"
#include "../util/text_reader.h"
#include "../util/paths.h"

#include <math.h>


#define TXTP_FILENAME_MAX 1024
#define TXTP_MIXING_MAX 512
#define TXTP_GROUP_MODE_SEGMENTED 'S'
#define TXTP_GROUP_MODE_LAYERED 'L'
#define TXTP_GROUP_MODE_RANDOM 'R'
#define TXTP_GROUP_RANDOM_ALL '-'
#define TXTP_GROUP_REPEAT 'R'
#define TXTP_POSITION_LOOPS 'L'

#define TXTP_BODY_INTRO 1
#define TXTP_BODY_MAIN 2
#define TXTP_BODY_OUTRO 3


/* mixing info */
typedef enum {
    MIX_SWAP,
    MIX_ADD,
    MIX_ADD_VOLUME,
    MIX_VOLUME,
    MIX_LIMIT,
    MIX_DOWNMIX,
    MIX_KILLMIX,
    MIX_UPMIX,
    MIX_FADE,

    MACRO_VOLUME,
    MACRO_TRACK,
    MACRO_LAYER,
    MACRO_CROSSTRACK,
    MACRO_CROSSLAYER,
    MACRO_DOWNMIX,

} txtp_mix_t;

typedef struct {
    txtp_mix_t command;
    /* common */
    int ch_dst;
    int ch_src;
    double vol;

    /* fade envelope */
    double vol_start;
    double vol_end;
    char shape;
    int32_t sample_pre;
    int32_t sample_start;
    int32_t sample_end;
    int32_t sample_post;
    double time_pre;
    double time_start;
    double time_end;
    double time_post;
    double position;
    char position_type;

    /* macros */
    int max;
    uint32_t mask;
    char mode;
} txtp_mix_data_t;


typedef struct {
    /* main entry */
    char filename[TXTP_FILENAME_MAX];
    bool silent;

    /* TXTP settings (applied at the end) */
    int range_start;
    int range_end;
    int subsong;

    uint32_t channel_mask;

    int mixing_count;
    txtp_mix_data_t mixing[TXTP_MIXING_MAX];

    play_config_t config;

    int sample_rate;

    int loop_install_set;
    int loop_end_max;
    double loop_start_second;
    int32_t loop_start_sample;
    double loop_end_second;
    int32_t loop_end_sample;
    /* flags */
    int loop_anchor_start;
    int loop_anchor_end;

    int trim_set;
    double trim_second;
    int32_t trim_sample;

    int body_mode;

} txtp_entry_t;


typedef struct {
    int position;
    char type;
    int count;
    char repeat;
    int selected;

    txtp_entry_t entry;

} txtp_group_t;

typedef struct {
    txtp_entry_t* entry;
    size_t entry_count;
    size_t entry_max;

    txtp_group_t* group;
    size_t group_count;
    size_t group_max;
    int group_pos; /* entry counter for groups */

    VGMSTREAM** vgmstream;
    size_t vgmstream_count;

    uint32_t loop_start_segment;
    uint32_t loop_end_segment;
    bool is_loop_keep;
    bool is_loop_auto;

    txtp_entry_t default_entry;
    int default_entry_set;

    bool is_segmented;
    bool is_layered;
    bool is_single;
} txtp_header_t;

txtp_header_t* txtp_parse(STREAMFILE* sf);
bool txtp_process(txtp_header_t* txtp, STREAMFILE* sf);

void txtp_clean(txtp_header_t* txtp);
void txtp_add_mixing(txtp_entry_t* entry, txtp_mix_data_t* mix, txtp_mix_t command);
void txtp_copy_config(play_config_t* dst, play_config_t* src);
#endif
