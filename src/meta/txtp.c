#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../mixing.h"
#include "../plugins.h"
#include "../util/text_reader.h"

#include <math.h>


#define TXT_LINE_MAX 2048 /* some wwise .txtp get wordy */
#define TXT_LINE_KEY_MAX 128
#define TXT_LINE_VAL_MAX (TXT_LINE_MAX - TXT_LINE_KEY_MAX)
#define TXTP_MIXING_MAX 512
#define TXTP_GROUP_MODE_SEGMENTED 'S'
#define TXTP_GROUP_MODE_LAYERED 'L'
#define TXTP_GROUP_MODE_RANDOM 'R'
#define TXTP_GROUP_RANDOM_ALL '-'
#define TXTP_GROUP_REPEAT 'R'
#define TXTP_POSITION_LOOPS 'L'

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
} txtp_mix_data;


typedef struct {
    /* main entry */
    char filename[TXT_LINE_MAX];
    int silent;

    /* TXTP settings (applied at the end) */
    int range_start;
    int range_end;
    int subsong;

    uint32_t channel_mask;

    int mixing_count;
    txtp_mix_data mixing[TXTP_MIXING_MAX];

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

} txtp_entry;


typedef struct {
    int position;
    char type;
    int count;
    char repeat;
    int selected;

    txtp_entry entry;

} txtp_group;

typedef struct {
    txtp_entry* entry;
    size_t entry_count;
    size_t entry_max;

    txtp_group* group;
    size_t group_count;
    size_t group_max;
    int group_pos; /* entry counter for groups */

    VGMSTREAM** vgmstream;
    size_t vgmstream_count;

    uint32_t loop_start_segment;
    uint32_t loop_end_segment;
    int is_loop_keep;
    int is_loop_auto;

    txtp_entry default_entry;
    int default_entry_set;

    int is_segmented;
    int is_layered;
    int is_single;
} txtp_header;

static txtp_header* parse_txtp(STREAMFILE* sf);
static int parse_entries(txtp_header* txtp, STREAMFILE* sf);
static int parse_groups(txtp_header* txtp);
static void clean_txtp(txtp_header* txtp, int fail);
static void apply_settings(VGMSTREAM* vgmstream, txtp_entry* current);
void add_mixing(txtp_entry* cfg, txtp_mix_data* mix, txtp_mix_t command);


/* TXTP - an artificial playlist-like format to play files with segments/layers/config */
VGMSTREAM* init_vgmstream_txtp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    txtp_header* txtp = NULL;
    int ok;


    /* checks */
    if (!check_extensions(sf, "txtp"))
        goto fail;

    /* read .txtp with all files and settings */
    txtp = parse_txtp(sf);
    if (!txtp) goto fail;

    /* process files in the .txtp */
    ok = parse_entries(txtp, sf);
    if (!ok) goto fail;

    /* group files into layouts */
    ok = parse_groups(txtp);
    if (!ok) goto fail;


    /* may happen if using mixed mode but some files weren't grouped */
    if (txtp->vgmstream_count != 1) {
        VGM_LOG("TXTP: wrong final vgmstream count %i\n", txtp->vgmstream_count);
        goto fail;
    }

    /* should result in a final, single vgmstream possibly containing multiple vgmstreams */
    vgmstream = txtp->vgmstream[0];

    /* flags for title config */
    vgmstream->config.is_txtp = 1;
    vgmstream->config.is_mini_txtp = (get_streamfile_size(sf) == 0);

    clean_txtp(txtp, 0);
    return vgmstream;
fail:
    clean_txtp(txtp, 1);
    return NULL;
}

static void clean_txtp(txtp_header* txtp, int fail) {
    int i, start;

    if (!txtp)
        return;

    /* returns first vgmstream on success so it's not closed */
    start = fail ? 0 : 1;

    for (i = start; i < txtp->vgmstream_count; i++) {
        close_vgmstream(txtp->vgmstream[i]);
    }

    free(txtp->vgmstream);
    free(txtp->group);
    free(txtp->entry);
    free(txtp);
}

//todo fragment parser later

/*******************************************************************************/
/* ENTRIES                                                                     */
/*******************************************************************************/

static int parse_silents(txtp_header* txtp) {
    int i;
    VGMSTREAM* v_base = NULL;

    /* silents use same channels as close files */
    for (i = 0; i < txtp->vgmstream_count; i++) {
        if (!txtp->entry[i].silent) {
            v_base = txtp->vgmstream[i];
            break;
        }
    }

    /* actually open silents */
    for (i = 0; i < txtp->vgmstream_count; i++) {
        if (!txtp->entry[i].silent)
            continue;

        txtp->vgmstream[i] = init_vgmstream_silence_base(v_base);
        if (!txtp->vgmstream[i]) goto fail;

        apply_settings(txtp->vgmstream[i], &txtp->entry[i]);
    }

    return 1;
fail:
    return 0;
}

static int is_silent(const char* fn) {
    /* should also contain "." in the filename for commands with seconds ("1.0") to work */
    return fn[0] == '?';
}

static int is_absolute(const char* fn) {
    return fn[0] == '/' || fn[0] == '\\'  || fn[1] == ':';
}

/* open all entries and apply settings to resulting VGMSTREAMs */
static int parse_entries(txtp_header* txtp, STREAMFILE* sf) {
    int i;
    int has_silents = 0;


    if (txtp->entry_count == 0)
        goto fail;

    txtp->vgmstream = calloc(txtp->entry_count, sizeof(VGMSTREAM*));
    if (!txtp->vgmstream) goto fail;

    txtp->vgmstream_count = txtp->entry_count;


    /* open all entry files first as they'll be modified by modes */
    for (i = 0; i < txtp->vgmstream_count; i++) {
        STREAMFILE* temp_sf = NULL;
        const char* filename = txtp->entry[i].filename;

        /* silent entry ignore */
        if (is_silent(filename)) {
            txtp->entry[i].silent = 1;
            has_silents = 1;
            continue;
        }

        /* absolute paths are detected for convenience, but since it's hard to unify all OSs
         * and plugins, they aren't "officially" supported nor documented, thus may or may not work */
        if (is_absolute(filename))
            temp_sf = open_streamfile(sf, filename); /* from path as is */
        else
            temp_sf = open_streamfile_by_filename(sf, filename); /* from current path */
        if (!temp_sf) {
            vgm_logi("TXTP: cannot open %s\n", filename);
            goto fail;
        }
        temp_sf->stream_index = txtp->entry[i].subsong;

        txtp->vgmstream[i] = init_vgmstream_from_STREAMFILE(temp_sf);
        close_streamfile(temp_sf);
        if (!txtp->vgmstream[i]) {
            vgm_logi("TXTP: cannot parse %s#%i\n", filename, txtp->entry[i].subsong);
            goto fail;
        }

        apply_settings(txtp->vgmstream[i], &txtp->entry[i]);
    }

    if (has_silents) {
        if (!parse_silents(txtp))
            goto fail;
    }

    return 1;
fail:
    return 0;
}


/*******************************************************************************/
/* GROUPS                                                                      */
/*******************************************************************************/

static void update_vgmstream_list(VGMSTREAM* vgmstream, txtp_header* txtp, int position, int count) {
    int i;

    //;VGM_LOG("TXTP: compact position=%i count=%i, vgmstreams=%i\n", position, count, txtp->vgmstream_count);

    /* sets and compacts vgmstream list pulling back all following entries */
    txtp->vgmstream[position] = vgmstream;
    for (i = position + count; i < txtp->vgmstream_count; i++) {
        //;VGM_LOG("TXTP: copy %i to %i\n", i, i + 1 - count);
        txtp->vgmstream[i + 1 - count] = txtp->vgmstream[i];
        txtp->entry[i + 1 - count] = txtp->entry[i]; /* memcpy old settings for other groups */
    }

    /* list can only become smaller, no need to alloc/free/etc */
    txtp->vgmstream_count = txtp->vgmstream_count + 1 - count;
    //;VGM_LOG("TXTP: compact vgmstreams=%i\n", txtp->vgmstream_count);
}

static int find_loop_anchors(txtp_header* txtp, int position, int count, int* p_loop_start, int* p_loop_end) {
    int loop_start = 0, loop_end = 0;
    int i, j;

    //;VGM_LOG("TXTP: find loop anchors from %i to %i\n", position, count);

    for (i = position, j = 0; i < position + count; i++, j++) {
        /* catch first time anchors appear only, also logic elsewhere also uses +1 */
        if (txtp->entry[i].loop_anchor_start && !loop_start) {
            loop_start = j + 1;
        }
        if (txtp->entry[i].loop_anchor_end && !loop_end) {
            loop_end = j + 1;
        }
    }

    if (loop_start) {
        if (!loop_end)
            loop_end = count;
        *p_loop_start = loop_start;
        *p_loop_end = loop_end;
        //;VGM_LOG("TXTP: loop anchors %i, %i\n", loop_start, loop_end);
        return 1;
    }

    return 0;
}


static int make_group_segment(txtp_header* txtp, txtp_group* grp, int position, int count) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data *data_s = NULL;
    int i, loop_flag = 0;
    int loop_start = 0, loop_end = 0;


    /* allowed for actual groups (not final "mode"), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return 1;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored segment position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return 1;
    }


    /* set loops with "anchors" (this allows loop config inside groups, not just in the final group,
     * which is sometimes useful when paired with random/selectable groups or loop times) */
    if (find_loop_anchors(txtp, position, count, &loop_start, &loop_end)) {
        loop_flag = (loop_start > 0 && loop_start <= count);
    }
    /* loop segment settings only make sense if this group becomes final vgmstream */
    else if (position == 0 && txtp->vgmstream_count == count) {
        loop_start = txtp->loop_start_segment;
        loop_end = txtp->loop_end_segment;

        if (loop_start && !loop_end) {
            loop_end = count;
        }
        else if (txtp->is_loop_auto) { /* auto set to last segment */
            loop_start = count;
            loop_end = count;
        }
        loop_flag = (loop_start > 0 && loop_start <= count);
    }


    /* init layout */
    data_s = init_layout_segmented(count);
    if (!data_s) goto fail;

    /* copy each subfile */
    for (i = 0; i < count; i++) {
        data_s->segments[i] = txtp->vgmstream[i + position];
        txtp->vgmstream[i + position] = NULL; /* will be freed by layout */
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_segmented(data_s))
        goto fail;

    /* build the layout VGMSTREAM */
    vgmstream = allocate_segmented_vgmstream(data_s, loop_flag, loop_start - 1, loop_end - 1);
    if (!vgmstream) goto fail;

    /* custom meta name if all parts don't match */
    for (i = 0; i < count; i++) {
        if (vgmstream->meta_type != data_s->segments[i]->meta_type) {
            vgmstream->meta_type = meta_TXTP;
            break;
        }
    }

    /* fix loop keep */
    if (loop_flag && txtp->is_loop_keep) {
        int32_t current_samples = 0;
        for (i = 0; i < count; i++) {
            if (loop_start == i+1 /*&& data_s->segments[i]->loop_start_sample*/) {
                vgmstream->loop_start_sample = current_samples + data_s->segments[i]->loop_start_sample;
            }

            current_samples += data_s->segments[i]->num_samples;

            if (loop_end == i+1 && data_s->segments[i]->loop_end_sample) {
                vgmstream->loop_end_sample = current_samples - data_s->segments[i]->num_samples + data_s->segments[i]->loop_end_sample;
            }
        }
    }


    /* set new vgmstream and reorder positions */
    update_vgmstream_list(vgmstream, txtp, position, count);


    /* special "whole loop" settings */
    if (grp && grp->entry.loop_anchor_start == 1) {
        grp->entry.config.config_set = 1;
        grp->entry.config.really_force_loop = 1;
    }

    return 1;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_segmented(data_s);
    return 0;
}

static int make_group_layer(txtp_header* txtp, txtp_group* grp, int position, int count) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data_l = NULL;
    int i;


    /* allowed for actual groups (not final mode), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return 1;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored layer position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return 1;
    }


    /* init layout */
    data_l = init_layout_layered(count);
    if (!data_l) goto fail;

    /* copy each subfile */
    for (i = 0; i < count; i++) {
        data_l->layers[i] = txtp->vgmstream[i + position];
        txtp->vgmstream[i + position] = NULL; /* will be freed by layout */
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_layered(data_l))
        goto fail;

    /* build the layout VGMSTREAM */
    vgmstream = allocate_layered_vgmstream(data_l);
    if (!vgmstream) goto fail;

    /* custom meta name if all parts don't match */
    for (i = 0; i < count; i++) {
        if (vgmstream->meta_type != data_l->layers[i]->meta_type) {
            vgmstream->meta_type = meta_TXTP;
            break;
        }
    }

    /* set new vgmstream and reorder positions */
    update_vgmstream_list(vgmstream, txtp, position, count);


    /* special "whole loop" settings (also loop if this group becomes final vgmstream) */
    if (grp && (grp->entry.loop_anchor_start == 1
            || (position == 0 && txtp->vgmstream_count == count && txtp->is_loop_auto))) {
        grp->entry.config.config_set = 1;
        grp->entry.config.really_force_loop = 1;
    }

    return 1;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data_l);
    return 0;
}

static int make_group_random(txtp_header* txtp, txtp_group* grp, int position, int count, int selected) {
    VGMSTREAM* vgmstream = NULL;
    int i;

    /* allowed for actual groups (not final mode), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return 1;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored random position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return 1;
    }

    /* 0=actually random for fun and testing, but undocumented since random music is kinda weird, may change anytime
     * (plus foobar caches song duration unless .txtp is modifies, so it can get strange if randoms are too different) */
    if (selected < 0) {
        static int random_seed = 0;
        srand((unsigned)txtp + random_seed++); /* whatevs */
        selected = (rand() % count); /* 0..count-1 */
        //;VGM_LOG("TXTP: autoselected random %i\n", selected);
    }

    if (selected < 0 || selected > count) {
        goto fail;
    }

    if (selected == count) {
        /* special case meaning "select all", basically for quick testing and clearer Wwise */
        if (!make_group_segment(txtp, grp, position, count))
            goto fail;
        vgmstream = txtp->vgmstream[position];
    }
    else {
        /* get selected and remove non-selected */
        vgmstream = txtp->vgmstream[position + selected];
        txtp->vgmstream[position + selected] = NULL;
        for (i = 0; i < count; i++) {
            close_vgmstream(txtp->vgmstream[i + position]);
        }

        /* set new vgmstream and reorder positions */
        update_vgmstream_list(vgmstream, txtp, position, count);
    }


    /* special "whole loop" settings */
    if (grp && grp->entry.loop_anchor_start == 1) {
        grp->entry.config.config_set = 1;
        grp->entry.config.really_force_loop = 1;
    }

    /* force selected vgmstream to be a segment when not a group already, and
     * group + vgmstream has config (AKA must loop/modify over the result) */
    //todo could optimize to not generate segment in some cases?
    if (grp &&
            !(vgmstream->layout_type == layout_layered || vgmstream->layout_type == layout_segmented) &&
            (grp->entry.config.config_set && vgmstream->config.config_set) ) {
        if (!make_group_segment(txtp, grp, position, 1))
            goto fail;
    }

    return 1;
fail:
    close_vgmstream(vgmstream);
    return 0;
}

static int parse_groups(txtp_header* txtp) {
    int i;

    /* detect single files before grouping */
    if (txtp->group_count == 0 && txtp->vgmstream_count == 1) {
        txtp->is_single = 1;
        txtp->is_segmented = 0;
        txtp->is_layered = 0;
    }

    /* group files as needed */
    for (i = 0; i < txtp->group_count; i++) {
        txtp_group *grp = &txtp->group[i];
        int pos, groups;

        //;VGM_LOG("TXTP: apply group %i%c%i%c\n",txtp->group[i].position,txtp->group[i].type,txtp->group[i].count,txtp->group[i].repeat);

        /* special meaning of "all files" */
        if (grp->position < 0 || grp->position >= txtp->vgmstream_count)
            grp->position = 0;
        if (grp->count <= 0)
            grp->count = txtp->vgmstream_count - grp->position;

        /* repeats N groups (trailing files are not grouped) */
        if (grp->repeat == TXTP_GROUP_REPEAT) {
            groups = ((txtp->vgmstream_count - grp->position) / grp->count);
        }
        else {
            groups = 1;
        }

        /* as groups are compacted position goes 1 by 1 */
        for (pos = grp->position; pos < grp->position + groups; pos++) {
            //;VGM_LOG("TXTP: group=%i, count=%i, groups=%i\n", pos, grp->count, groups);
            switch(grp->type) {
                case TXTP_GROUP_MODE_LAYERED:
                    if (!make_group_layer(txtp, grp, pos, grp->count))
                        goto fail;
                    break;
                case TXTP_GROUP_MODE_SEGMENTED:
                    if (!make_group_segment(txtp, grp, pos, grp->count))
                        goto fail;
                    break;
                case TXTP_GROUP_MODE_RANDOM:
                    if (!make_group_random(txtp, grp, pos, grp->count, grp->selected))
                        goto fail;
                    break;
                default:
                    goto fail;
            }
        }


        /* group may also have settings (like downmixing) */
        apply_settings(txtp->vgmstream[grp->position], &grp->entry);
        txtp->entry[grp->position] = grp->entry; /* memcpy old settings for subgroups */
    }

    /* final tweaks (should be integrated with the above?) */
    if (txtp->is_layered) {
        if (!make_group_layer(txtp, NULL, 0, txtp->vgmstream_count))
            goto fail;
    }
    if (txtp->is_segmented) {
        if (!make_group_segment(txtp, NULL, 0, txtp->vgmstream_count))
            goto fail;
    }
    if (txtp->is_single) {
        /* special case of setting start_segment to force/overwrite looping
         * (better to use #E but left for compatibility with older TXTPs) */
        if (txtp->loop_start_segment == 1 && !txtp->loop_end_segment) {
            //todo try look settings
            //txtp->default_entry.config.config_set = 1;
            //txtp->default_entry.config.really_force_loop = 1;
            vgmstream_force_loop(txtp->vgmstream[0], 1, txtp->vgmstream[0]->loop_start_sample, txtp->vgmstream[0]->num_samples);
        }
    }

    /* apply default settings to the resulting file */
    if (txtp->default_entry_set) {
        apply_settings(txtp->vgmstream[0], &txtp->default_entry);
    }

    return 1;
fail:
    return 0;
}


/*******************************************************************************/
/* CONFIG                                                                      */
/*******************************************************************************/

static void copy_flag(int* dst_flag, int* src_flag) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
}

static void copy_secs(int* dst_flag, double* dst_secs, int* src_flag, double* src_secs) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_secs = *src_secs;
}

static void copy_time(int* dst_flag, int32_t* dst_time, double* dst_time_s, int* src_flag, int32_t* src_time, double* src_time_s) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_time = *src_time;
    *dst_time_s = *src_time_s;
}

static void copy_config(play_config_t* dst, play_config_t* src) {
    if (!src->config_set)
        return;

    dst->config_set = 1;
    copy_flag(&dst->play_forever,       &src->play_forever);
    copy_flag(&dst->ignore_fade,        &src->ignore_fade);
    copy_flag(&dst->force_loop,         &src->force_loop);
    copy_flag(&dst->really_force_loop,  &src->really_force_loop);
    copy_flag(&dst->ignore_loop,        &src->ignore_loop);
    copy_secs(&dst->loop_count_set,     &dst->loop_count,   &src->loop_count_set,  &src->loop_count);
    copy_secs(&dst->fade_time_set,      &dst->fade_time,    &src->fade_time_set,   &src->fade_time);
    copy_secs(&dst->fade_delay_set,     &dst->fade_delay,   &src->fade_delay_set,  &src->fade_delay);
    copy_time(&dst->pad_begin_set,      &dst->pad_begin,    &dst->pad_begin_s,     &src->pad_begin_set,     &src->pad_begin,    &src->pad_begin_s);
    copy_time(&dst->pad_end_set,        &dst->pad_end,      &dst->pad_end_s,       &src->pad_end_set,       &src->pad_end,      &src->pad_end_s);
    copy_time(&dst->trim_begin_set,     &dst->trim_begin,   &dst->trim_begin_s,    &src->trim_begin_set,    &src->trim_begin,   &src->trim_begin_s);
    copy_time(&dst->trim_end_set,       &dst->trim_end,     &dst->trim_end_s,      &src->trim_end_set,      &src->trim_end,     &src->trim_end_s);
    copy_time(&dst->body_time_set,      &dst->body_time,    &dst->body_time_s,     &src->body_time_set,     &src->body_time,    &src->body_time_s);
}

#if 0
static void init_config(VGMSTREAM* vgmstream) {
    play_config_t* cfg = &vgmstream->config;

    //todo only on segmented/layered?
    if (cfg->play_forever
            cfg->loop_count_set || cfg->fade_time_set || cfg->fade_delay_set ||
            cfg->pad_begin_set || cfg->pad_end_set || cfg->trim_begin_set || cfg->trim_end_set ||
            cfg->body_time_set) {
        VGM_LOG("setup!\n");

    }
}
#endif

static void apply_settings(VGMSTREAM* vgmstream, txtp_entry* current) {

    /* base settings */
    if (current->sample_rate > 0) {
        vgmstream->sample_rate = current->sample_rate;
    }

    if (current->loop_install_set) {
        if (current->loop_start_second > 0 || current->loop_end_second > 0) {
            current->loop_start_sample = current->loop_start_second * vgmstream->sample_rate;
            current->loop_end_sample = current->loop_end_second * vgmstream->sample_rate;
            if (current->loop_end_sample > vgmstream->num_samples &&
                    current->loop_end_sample - vgmstream->num_samples <= 0.1 * vgmstream->sample_rate)
                current->loop_end_sample = vgmstream->num_samples; /* allow some rounding leeway */
        }

        if (current->loop_end_max) {
            current->loop_end_sample  = vgmstream->num_samples;
        }

        vgmstream_force_loop(vgmstream, current->loop_install_set, current->loop_start_sample, current->loop_end_sample);
    }

    if (current->trim_set) {
        if (current->trim_second != 0.0) {
            /* trim sample can become 0 here when second is too small (rounded) */
            current->trim_sample = (double)current->trim_second * (double)vgmstream->sample_rate;
        }

        if (current->trim_sample < 0) {
            vgmstream->num_samples += current->trim_sample; /* trim from end (add negative) */
        }
        else if (current->trim_sample > 0 && vgmstream->num_samples > current->trim_sample) {
            vgmstream->num_samples = current->trim_sample; /* trim to value >0 */
        }

        /* readjust after triming if it went over (could check for more edge cases but eh) */
        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }


    /* add macro to mixing list */
    if (current->channel_mask) {
        int ch;
        for (ch = 0; ch < vgmstream->channels; ch++) {
            if (!((current->channel_mask >> ch) & 1)) {
                txtp_mix_data mix = {0};
                mix.ch_dst = ch + 1;
                mix.vol = 0.0f;
                add_mixing(current, &mix, MIX_VOLUME);
            }
        }
    }

    /* copy mixing list (should be done last as some mixes depend on config) */
    if (current->mixing_count > 0) {
        int m, position_samples;

        for (m = 0; m < current->mixing_count; m++) {
            txtp_mix_data *mix = &current->mixing[m];

            switch(mix->command) {
                /* base mixes */
                case MIX_SWAP:       mixing_push_swap(vgmstream, mix->ch_dst, mix->ch_src); break;
                case MIX_ADD:        mixing_push_add(vgmstream, mix->ch_dst, mix->ch_src, 1.0); break;
                case MIX_ADD_VOLUME: mixing_push_add(vgmstream, mix->ch_dst, mix->ch_src, mix->vol); break;
                case MIX_VOLUME:     mixing_push_volume(vgmstream, mix->ch_dst, mix->vol); break;
                case MIX_LIMIT:      mixing_push_limit(vgmstream, mix->ch_dst, mix->vol); break;
                case MIX_UPMIX:      mixing_push_upmix(vgmstream, mix->ch_dst); break;
                case MIX_DOWNMIX:    mixing_push_downmix(vgmstream, mix->ch_dst); break;
                case MIX_KILLMIX:    mixing_push_killmix(vgmstream, mix->ch_dst); break;
                case MIX_FADE:
                    /* Convert from time to samples now that sample rate is final.
                     * Samples and time values may be mixed though, so it's done for every
                     * value (if one is 0 the other will be too, though) */
                    if (mix->time_pre > 0.0)   mix->sample_pre = mix->time_pre * vgmstream->sample_rate;
                    if (mix->time_start > 0.0) mix->sample_start = mix->time_start * vgmstream->sample_rate;
                    if (mix->time_end > 0.0)   mix->sample_end = mix->time_end * vgmstream->sample_rate;
                    if (mix->time_post > 0.0)  mix->sample_post = mix->time_post * vgmstream->sample_rate;
                    /* convert special meaning too */
                    if (mix->time_pre < 0.0)   mix->sample_pre = -1;
                    if (mix->time_post < 0.0)  mix->sample_post = -1;

                    if (mix->position_type == TXTP_POSITION_LOOPS && vgmstream->loop_flag) {
                        int loop_pre = vgmstream->loop_start_sample;
                        int loop_samples = (vgmstream->loop_end_sample - vgmstream->loop_start_sample);

                        position_samples = loop_pre + loop_samples * mix->position;

                        if (mix->sample_pre >= 0) mix->sample_pre += position_samples;
                        mix->sample_start += position_samples;
                        mix->sample_end += position_samples;
                        if (mix->sample_post >= 0) mix->sample_post += position_samples;
                    }


                    mixing_push_fade(vgmstream, mix->ch_dst, mix->vol_start, mix->vol_end, mix->shape,
                            mix->sample_pre, mix->sample_start, mix->sample_end, mix->sample_post);
                    break;

                /* macro mixes */
                case MACRO_VOLUME:      mixing_macro_volume(vgmstream, mix->vol, mix->mask); break;
                case MACRO_TRACK:       mixing_macro_track(vgmstream, mix->mask); break;
                case MACRO_LAYER:       mixing_macro_layer(vgmstream, mix->max, mix->mask, mix->mode); break;
                case MACRO_CROSSTRACK:  mixing_macro_crosstrack(vgmstream, mix->max); break;
                case MACRO_CROSSLAYER:  mixing_macro_crosslayer(vgmstream, mix->max, mix->mode); break;
                case MACRO_DOWNMIX:     mixing_macro_downmix(vgmstream, mix->max); break;

                default:
                    break;
            }
        }
    }


    /* default play config (last after sample rate mods/mixing/etc) */
    copy_config(&vgmstream->config, &current->config);
    setup_state_vgmstream(vgmstream);
    /* config is enabled in layouts or externally (for compatibility, since we don't know yet if this
     * VGMSTREAM will part of a layout, or is enabled externally to not mess up plugins's calcs) */
}


/*******************************************************************************/
/* PARSER - HELPERS                                                            */
/*******************************************************************************/

/* sscanf 101: "matches = sscanf(string-from, string-commands, parameters...)"
 * - reads linearly and matches "%" commands to input parameters as found
 * - reads until string end (NULL) or not being able to match current parameter
 * - returns number of matched % parameters until stop, or -1 if no matches and reached string end
 * - must supply pointer param for every "%" in the string
 * - %d/f: match number until end or *non-number* (so "%d" reads "5t" as "5")
 * - %s: reads string (dangerous due to overflows and surprising as %s%d can't match numbers since string eats all chars)
 * - %[^(chars)] match string with chars not in the list (stop reading at those chars)
 * - %*(command) read but don't match (no need to supply parameterr)
 * - " ": ignore all spaces until next non-space
 * - other chars in string must exist: ("%dt t%dt" reads "5t  t5t" as "5" and "5", while "t5t 5t" matches only first "5")
 * - %n: special match (not counted in return value), chars consumed until that point (can appear and be set multiple times)
 */

static int get_double(const char* params, double *value, int *is_set) {
    int n, m;
    double temp;

    if (is_set) *is_set = 0;

    m = sscanf(params, " %lf%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    if (is_set) *is_set = 1;
    *value = temp;
    return n;
}

static int get_int(const char* params, int *value) {
    int n,m;
    int temp;

    m = sscanf(params, " %d%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    *value = temp;
    return n;
}

static int get_position(const char* params, double* value_f, char* value_type) {
    int n,m;
    double temp_f;
    char temp_c;

    /* test if format is position: N.n(type) */
    m = sscanf(params, " %lf%c%n", &temp_f,&temp_c,&n);
    if (m != 2 || temp_f < 0.0)
        return 0;
    /* test accepted chars as it will capture anything */
    if (temp_c != TXTP_POSITION_LOOPS)
        return 0;

    *value_f = temp_f;
    *value_type = temp_c;
    return n;
}

static int get_volume(const char* params, double *value, int *is_set) {
    int n, m;
    double temp_f;
    char temp_c1, temp_c2;

    if (is_set) *is_set = 0;

    /* test if format is NdB (decibels) */
    m = sscanf(params, " %lf%c%c%n", &temp_f, &temp_c1, &temp_c2, &n);
    if (m == 3 && temp_c1 == 'd' && (temp_c2 == 'B' || temp_c2 == 'b')) {
        /* dB 101:
         * - logaritmic scale
         *   - dB = 20 * log(percent / 100)
         *   - percent = pow(10, dB / 20)) * 100
         * - for audio: 100% = 0dB (base max volume of current file = reference dB)
         *   - negative dB decreases volume, positive dB increases
         * ex.
         *     200% = 20 * log(200 / 100) = +6.02059991328 dB
         *      50% = 20 * log( 50 / 100) = -6.02059991328 dB
         *      6dB = pow(10,  6 / 20) * 100 = +195.26231497 %
         *     -6dB = pow(10, -6 / 20) * 100 = +50.50118723362 %
         */

        if (is_set) *is_set = 1;
        *value = pow(10, temp_f / 20.0); /* dB to % where 1.0 = max */
        return n;
    }

    /* test if format is N.N (percent) */
    m = sscanf(params, " %lf%n", &temp_f, &n);
    if (m == 1) {
        if (is_set) *is_set = 1;
        *value = temp_f;
        return n;
    }

    return 0;
}

static int get_time(const char* params, double* value_f, int32_t* value_i) {
    int n,m;
    int temp_i1, temp_i2;
    double temp_f1, temp_f2;
    char temp_c;

    /* test if format is hour: N:N(.n) or N_N(.n) */
    m = sscanf(params, " %d%c%d%n", &temp_i1,&temp_c,&temp_i2,&n);
    if (m == 3 && (temp_c == ':' || temp_c == '_')) {
        m = sscanf(params, " %lf%c%lf%n", &temp_f1,&temp_c,&temp_f2,&n);
        if (m != 3 || /*temp_f1 < 0.0 ||*/ temp_f1 >= 60.0 || temp_f2 < 0.0 || temp_f2 >= 60.0)
            return 0;

        *value_f = temp_f1 * 60.0 + temp_f2;
        return n;
    }

    /* test if format is seconds: N.n */
    m = sscanf(params, " %d.%d%n", &temp_i1,&temp_i2,&n);
    if (m == 2) {
        m = sscanf(params, " %lf%n", &temp_f1,&n);
        if (m != 1 /*|| temp_f1 < 0.0*/)
            return 0;
        *value_f = temp_f1;
        return n;
    }

    /* test is format is hex samples: 0xN */
    m = sscanf(params, " 0x%x%n", &temp_i1,&n);
    if (m == 1) {
        /* allow negative samples for special meanings */
        //if (temp_i1 < 0)
        //    return 0;

        *value_i = temp_i1;
        return n;
    }

    /* assume format is samples: N */
    m = sscanf(params, " %d%n", &temp_i1,&n);
    if (m == 1) {
        /* allow negative samples for special meanings */
        //if (temp_i1 < 0)
        //    return 0;

        *value_i = temp_i1;
        return n;
    }

    return 0;
}

static int get_time_f(const char* params, double* value_f, int32_t* value_i, int* flag) {
    int n = get_time(params, value_f, value_i);
    if (n > 0)
        *flag = 1;
    return n;
}

static int get_bool(const char* params, int* value) {
    int n,m;
    char temp;

    n = 0; /* init as it's not matched if c isn't */
    m = sscanf(params, " %c%n", &temp, &n);
    if (m >= 1 && !(temp == '#' || temp == '\r' || temp == '\n'))
        return 0; /* ignore if anything non-space/comment matched */

    if (m >= 1 && temp == '#')
        n--; /* don't consume separator when returning totals */
    *value = 1;
    return n;
}

static int get_mask(const char* params, uint32_t* value) {
    int n, m, total_n = 0;
    int temp1,temp2, r1, r2;
    int i;
    char cmd;
    uint32_t mask = *value;

    while (params[0] != '\0') {
        m = sscanf(params, " %c%n", &cmd,&n); /* consume comma */
        if (m == 1 && (cmd == ',' || cmd == '-')) { /* '-' is alt separator (space is ok too, implicitly) */
            params += n;
            continue;
        }

        m = sscanf(params, " %d%n ~ %d%n", &temp1,&n, &temp2,&n);
        if (m == 1) { /* single values */
            r1 = temp1 - 1;
            r2 = temp1 - 1;
        }
        else if (m == 2) { /* range */
            r1 = temp1 - 1;
            r2 = temp2 - 1;
        }
        else { /* no more matches */
            break;
        }

        if (n == 0 || r1 < 0 || r1 > 31 || r2 < 0 || r2 > 31)
            break;

        for (i = r1; i < r2 + 1; i++) {
            mask |= (1 << i);
        }

        params += n;
        total_n += n;

        if (params[0]== ',' || params[0]== '-')
            params++;
    }

    *value = mask;
    return total_n;
}


static int get_fade(const char* params, txtp_mix_data* mix, int* p_n) {
    int n, m, tn = 0;
    char type, separator;

    m = sscanf(params, " %d %c%n", &mix->ch_dst, &type, &n);
    if (m != 2 || n == 0) goto fail;
    params += n;
    tn += n;

    if (type == '^') {
        /* full definition */
        m = sscanf(params, " %lf ~ %lf = %c @%n", &mix->vol_start, &mix->vol_end, &mix->shape, &n);
        if (m != 3 || n == 0) goto fail;
        params += n;
        tn += n;

        n = get_time(params, &mix->time_pre, &mix->sample_pre);
        if (n == 0) goto fail;
        params += n;
        tn += n;

        m = sscanf(params, " %c%n", &separator, &n);
        if ( m != 1 || n == 0 || separator != '~') goto fail;
        params += n;
        tn += n;

        n = get_time(params, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        params += n;
        tn += n;

        m = sscanf(params, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '+') goto fail;
        params += n;
        tn += n;

        n = get_time(params, &mix->time_end, &mix->sample_end);
        if (n == 0) goto fail;
        params += n;
        tn += n;

        m = sscanf(params, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '~') goto fail;
        params += n;
        tn += n;

        n = get_time(params, &mix->time_post, &mix->sample_post);
        if (n == 0) goto fail;
        params += n;
        tn += n;
    }
    else {
        /* simplified definition */
        if (type == '{' || type == '(') {
            mix->vol_start = 0.0;
            mix->vol_end = 1.0;
        }
        else if (type == '}' || type == ')') {
            mix->vol_start = 1.0;
            mix->vol_end = 0.0;
        }
        else {
            goto fail;
        }

        mix->shape = type; /* internally converted */

        mix->time_pre = -1.0;
        mix->sample_pre = -1;

        n = get_position(params, &mix->position, &mix->position_type);
        //if (n == 0) goto fail; /* optional */
        params += n;
        tn += n;

        n = get_time(params, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        params += n;
        tn += n;

        m = sscanf(params, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '+') goto fail;
        params += n;
        tn += n;

        n = get_time(params, &mix->time_end, &mix->sample_end);
        if (n == 0) goto fail;
        params += n;
        tn += n;

        mix->time_post = -1.0;
        mix->sample_post = -1;
    }

    mix->time_end = mix->time_start + mix->time_end; /* defined as length */

    *p_n = tn;
    return 1;
fail:
    return 0;
}

/*******************************************************************************/
/* PARSER - MAIN                                                               */
/*******************************************************************************/

void add_mixing(txtp_entry* entry, txtp_mix_data* mix, txtp_mix_t command) {
    if (entry->mixing_count + 1 > TXTP_MIXING_MAX) {
        VGM_LOG("TXTP: too many mixes\n");
        return;
    }

    /* parser reads ch1 = first, but for mixing code ch0 = first
     * (if parser reads ch0 here it'll become -1 with meaning of "all channels" in mixing code) */
    mix->ch_dst--;
    mix->ch_src--;
    mix->command = command;

    entry->mixing[entry->mixing_count] = *mix; /* memcpy'ed */
    entry->mixing_count++;
}

static void add_settings(txtp_entry* current, txtp_entry* entry, const char* filename) {

    /* don't memcopy to allow list additions and ignore values not set, as current can be "default" settings */
    //*current = *cfg;

    if (filename)
        strcpy(current->filename, filename);


    /* play config */
    copy_config(&current->config, &entry->config);

    /* file settings */
    if (entry->subsong)
        current->subsong = entry->subsong;

    if (entry->sample_rate > 0)
        current->sample_rate = entry->sample_rate;

    if (entry->channel_mask)
        current->channel_mask = entry->channel_mask;

    if (entry->loop_install_set) {
        current->loop_install_set = entry->loop_install_set;
        current->loop_end_max = entry->loop_end_max;
        current->loop_start_sample = entry->loop_start_sample;
        current->loop_start_second = entry->loop_start_second;
        current->loop_end_sample = entry->loop_end_sample;
        current->loop_end_second = entry->loop_end_second;
    }

    if (entry->trim_set) {
        current->trim_set = entry->trim_set;
        current->trim_second = entry->trim_second;
        current->trim_sample = entry->trim_sample;
    }

    if (entry->mixing_count > 0) {
        int i;
        for (i = 0; i < entry->mixing_count; i++) {
            current->mixing[current->mixing_count] = entry->mixing[i];
            current->mixing_count++;
        }
    }

    current->loop_anchor_start = entry->loop_anchor_start;
    current->loop_anchor_end = entry->loop_anchor_end;
}

//TODO use
static inline int is_match(const char* str1, const char* str2) {
    return strcmp(str1, str2) == 0;
}

static void parse_params(txtp_entry* entry, char* params) {
    /* parse params: #(commands) */
    int n, nc, nm, mc;
    char command[TXT_LINE_MAX];
    play_config_t* tcfg = &entry->config;

    entry->range_start = 0;
    entry->range_end = 1;

    while (params != NULL) {
        /* position in next #(command) */
        params = strchr(params, '#');
        if (!params) break;
        //;VGM_LOG("TXTP: params='%s'\n", params);

        /* get command until next space/number/comment/end */
        command[0] = '\0';
        mc = sscanf(params, "#%n%[^ #0-9\r\n]%n", &nc, command, &nc);
        //;VGM_LOG("TXTP:  command='%s', nc=%i, mc=%i\n", command, nc, mc);
        if (mc <= 0 && nc == 0) break;

        params[0] = '\0'; //todo don't modify input string and properly calculate filename end

        params += nc; /* skip '#' and command */

        /* check command string (though at the moment we only use single letters) */
        if (strcmp(command,"c") == 0) {
            /* channel mask: file.ext#c1,2 = play channels 1,2 and mutes rest */

            params += get_mask(params, &entry->channel_mask);
            //;VGM_LOG("TXTP:   channel_mask ");{int i; for (i=0;i<16;i++)VGM_LOG("%i ",(entry->channel_mask>>i)&1);}VGM_LOG("\n");
        }
        else if (strcmp(command,"m") == 0) {
            /* channel mixing: file.ext#m(sub-command),(sub-command),etc */
            char cmd;

            while (params[0] != '\0') {
                txtp_mix_data mix = {0};

                //;VGM_LOG("TXTP: subcommand='%s'\n", params);

                //todo use strchr instead?
                if (sscanf(params, " %c%n", &cmd, &n) == 1 && n != 0 && cmd == ',') {
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d - %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i-%i\n", mix.ch_dst, mix.ch_src);
                    add_mixing(entry, &mix, MIX_SWAP); /* N-M: swaps M with N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d + %d * %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0) ||
                    (sscanf(params, " %d + %d x %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i+%i*%f\n", mix.ch_dst, mix.ch_src, mix.vol);
                    add_mixing(entry, &mix, MIX_ADD_VOLUME); /* N+M*V: mixes M*volume to N */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d + %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i+%i\n", mix.ch_dst, mix.ch_src);
                    add_mixing(entry, &mix, MIX_ADD); /* N+M: mixes M to N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d * %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0) ||
                    (sscanf(params, " %d x %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i*%f\n", mix.ch_dst, mix.vol);
                    add_mixing(entry, &mix, MIX_VOLUME); /* N*V: changes volume of N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d = %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i=%f\n", mix.ch_dst, mix.vol);
                    add_mixing(entry, &mix, MIX_LIMIT); /* N=V: limits volume of N */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'D') {
                    //;VGM_LOG("TXTP:   mix %iD\n", mix.ch_dst);
                    add_mixing(entry, &mix, MIX_KILLMIX); /* ND: downmix N and all following channels */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'd') {
                    //;VGM_LOG("TXTP:   mix %id\n", mix.ch_dst);
                    add_mixing(entry, &mix, MIX_DOWNMIX);/* Nd: downmix N only */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'u') {
                    //;VGM_LOG("TXTP:   mix %iu\n", mix.ch_dst);
                    add_mixing(entry, &mix, MIX_UPMIX); /* Nu: upmix N */
                    params += n;
                    continue;
                }

                if (get_fade(params, &mix, &n) != 0) {
                    //;VGM_LOG("TXTP:   fade %d^%f~%f=%c@%f~%f+%f~%f\n",
                    //        mix.ch_dst, mix.vol_start, mix.vol_end, mix.shape,
                    //        mix.time_pre, mix.time_start, mix.time_end, mix.time_post);
                    add_mixing(entry, &mix, MIX_FADE); /* N^V1~V2@T1~T2+T3~T4: fades volumes between positions */
                    params += n;
                    continue;
                }

                break; /* unknown mix/new command/end */
           }
        }
        else if (strcmp(command,"s") == 0 || (nc == 1 && params[0] >= '0' && params[0] <= '9')) {
            /* subsongs: file.ext#s2 = play subsong 2, file.ext#2~10 = play subsong range */
            int subsong_start = 0, subsong_end = 0;

            //todo also advance params?
            if (sscanf(params, " %d ~ %d", &subsong_start, &subsong_end) == 2) {
                if (subsong_start > 0 && subsong_end > 0) {
                    entry->range_start = subsong_start-1;
                    entry->range_end = subsong_end;
                }
                //;VGM_LOG("TXTP:   subsong range %i~%i\n", range_start, range_end);
            }
            else if (sscanf(params, " %d", &subsong_start) == 1) {
                if (subsong_start > 0) {
                    entry->range_start = subsong_start-1;
                    entry->range_end = subsong_start;
                }
                //;VGM_LOG("TXTP:   subsong single %i-%i\n", range_start, range_end);
            }
            else { /* wrong setting, ignore */
                //;VGM_LOG("TXTP:   subsong none\n");
            }
        }

        /* play config */
        else if (strcmp(command,"i") == 0) {
            params += get_bool(params, &tcfg->ignore_loop);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"e") == 0) {
            params += get_bool(params, &tcfg->force_loop);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"E") == 0) {
            params += get_bool(params, &tcfg->really_force_loop);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"F") == 0) {
            params += get_bool(params, &tcfg->ignore_fade);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"L") == 0) {
            params += get_bool(params, &tcfg->play_forever);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"l") == 0) {
            params += get_double(params, &tcfg->loop_count, &tcfg->loop_count_set);
            if (tcfg->loop_count < 0)
                tcfg->loop_count_set = 0;
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"f") == 0) {
            params += get_double(params, &tcfg->fade_time, &tcfg->fade_time_set);
            if (tcfg->fade_time < 0)
                tcfg->fade_time_set = 0;
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"d") == 0) {
            params += get_double(params, &tcfg->fade_delay, &tcfg->fade_delay_set);
            if (tcfg->fade_delay < 0)
                tcfg->fade_delay_set = 0;
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"p") == 0) {
            params += get_time_f(params, &tcfg->pad_begin_s, &tcfg->pad_begin, &tcfg->pad_begin_set);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"P") == 0) {
            params += get_time_f(params, &tcfg->pad_end_s, &tcfg->pad_end, &tcfg->pad_end_set);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"r") == 0) {
            params += get_time_f(params, &tcfg->trim_begin_s, &tcfg->trim_begin, &tcfg->trim_begin_set);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"R") == 0) {
            params += get_time_f(params, &tcfg->trim_end_s, &tcfg->trim_end, &tcfg->trim_end_set);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"b") == 0) {
            params += get_time_f(params, &tcfg->body_time_s, &tcfg->body_time, &tcfg->body_time_set);
            tcfg->config_set = 1;
        }
        else if (strcmp(command,"B") == 0) {
            params += get_time_f(params, &tcfg->body_time_s, &tcfg->body_time, &tcfg->body_time_set);
            tcfg->config_set = 1;
            /* similar to 'b' but implies no fades */
            tcfg->fade_time_set = 1;
            tcfg->fade_time = 0;
            tcfg->fade_delay_set = 1;
            tcfg->fade_delay = 0;
        }

        /* other settings */
        else if (strcmp(command,"h") == 0) {
            params += get_int(params, &entry->sample_rate);
            //;VGM_LOG("TXTP:   sample_rate %i\n", cfg->sample_rate);
        }
        else if (strcmp(command,"I") == 0) {
            n = get_time(params,  &entry->loop_start_second, &entry->loop_start_sample);
            if (n > 0) { /* first value must exist */
                params += n;

                n = get_time(params,  &entry->loop_end_second, &entry->loop_end_sample);
                if (n == 0) { /* second value is optional */
                    entry->loop_end_max = 1;
                }

                params += n;
                entry->loop_install_set = 1;
            }

            //;VGM_LOG("TXTP:   loop_install %i (max=%i): %i %i / %f %f\n", entry->loop_install, entry->loop_end_max,
            //        entry->loop_start_sample, entry->loop_end_sample, entry->loop_start_second, entry->loop_end_second);
        }
        else if (strcmp(command,"t") == 0) {
            entry->trim_set = get_time(params,  &entry->trim_second, &entry->trim_sample);
            //;VGM_LOG("TXTP: trim %i - %f / %i\n", entry->trim_set, entry->trim_second, entry->trim_sample);
        }

        else if (is_match(command,"a") || is_match(command,"@loop")) {
            entry->loop_anchor_start = 1;
            //;VGM_LOG("TXTP: anchor start set\n");
        }
        else if (is_match(command,"A") || is_match(command,"@loop-end")) {
            entry->loop_anchor_end = 1;
            //;VGM_LOG("TXTP: anchor end set\n");
        }

        //todo cleanup
        /* macros */
        else if (is_match(command,"v") || is_match(command,"@volume")) {
            txtp_mix_data mix = {0};

            nm = get_volume(params, &mix.vol, NULL);
            params += nm;

            if (nm == 0) continue;

            nm = get_mask(params, &mix.mask);
            params += nm;

            add_mixing(entry, &mix, MACRO_VOLUME);
        }
        else if (strcmp(command,"@track") == 0 ||
                 strcmp(command,"C") == 0 ) {
            txtp_mix_data mix = {0};

            nm = get_mask(params, &mix.mask);
            params += nm;
            if (nm == 0) continue;

            add_mixing(entry, &mix, MACRO_TRACK);
        }
        else if (strcmp(command,"@layer-v") == 0 ||
                 strcmp(command,"@layer-b") == 0 ||
                 strcmp(command,"@layer-e") == 0) {
            txtp_mix_data mix = {0};

            nm = get_int(params, &mix.max);
            params += nm;

            if (nm > 0) { /* max is optional (auto-detects and uses max channels) */
                nm = get_mask(params, &mix.mask);
                params += nm;
            }

            mix.mode = command[7]; /* pass letter */
            add_mixing(entry, &mix, MACRO_LAYER);
        }
        else if (strcmp(command,"@crosslayer-v") == 0 ||
                 strcmp(command,"@crosslayer-b") == 0 ||
                 strcmp(command,"@crosslayer-e") == 0 ||
                 strcmp(command,"@crosstrack") == 0) {
            txtp_mix_data mix = {0};
            txtp_mix_t type;
            if (strcmp(command,"@crosstrack") == 0) {
                type = MACRO_CROSSTRACK;
            }
            else {
                type = MACRO_CROSSLAYER;
                mix.mode = command[12]; /* pass letter */
            }

            nm = get_int(params, &mix.max);
            params += nm;
            if (nm == 0) continue;

            add_mixing(entry, &mix, type);
        }
        else if (strcmp(command,"@downmix") == 0) {
            txtp_mix_data mix = {0};

            mix.max = 2; /* stereo only for now */
            //nm = get_int(params, &mix.max);
            //params += nm;
            //if (nm == 0) continue;

            add_mixing(entry, &mix, MACRO_DOWNMIX);
        }
        else if (params[nc] == ' ') {
            //;VGM_LOG("TXTP:   comment\n");
            break; /* comment, ignore rest */
        }
        else {
            //;VGM_LOG("TXTP:   unknown command\n");
            /* end, incorrect command, or possibly a comment or double ## comment too
             * (shouldn't fail for forward compatibility) */
            break;
        }
    }
}


static int add_group(txtp_header* txtp, char* line) {
    int n, m;
    txtp_group cfg = {0};
    int auto_pos = 0;
    char c;

    /* parse group: (position)(type)(count)(repeat)  #(commands) */
    //;VGM_LOG("TXTP: parse group '%s'\n", line);

    m = sscanf(line, " %c%n", &c, &n);
    if (m == 1 && c == '-') {
        auto_pos = 1;
        line += n;
    }

    m = sscanf(line, " %d%n", &cfg.position, &n);
    if (m == 1) {
        cfg.position--; /* externally 1=first but internally 0=first */
        line += n;
    }

    m = sscanf(line, " %c%n", &cfg.type, &n);
    if (m == 1) {
        line += n;
    }

    m = sscanf(line, " %d%n", &cfg.count, &n);
    if (m == 1) {
        line += n;
    }

    m = sscanf(line, " %c%n", &cfg.repeat, &n);
    if (m == 1 && cfg.repeat == TXTP_GROUP_REPEAT) {
        auto_pos = 0;
        line += n;
    }

    m = sscanf(line, " >%c%n", &c, &n);
    if (m == 1 && c == TXTP_GROUP_RANDOM_ALL) {
        cfg.type = TXTP_GROUP_MODE_RANDOM; /* usually R>- but allows L/S>- */
        cfg.selected = cfg.count; /* special meaning */
        line += n;
    }
    else {
        m = sscanf(line, " >%d%n", &cfg.selected, &n);
        if (m == 1) {
            cfg.type = TXTP_GROUP_MODE_RANDOM; /* usually R>1 but allows L/S>1 */
            cfg.selected--; /* externally 1=first but internally 0=first */
            line += n;
        }
        else if (cfg.type == TXTP_GROUP_MODE_RANDOM) {
            /* was a random but didn't select anything, just select all */
            cfg.selected = cfg.count;
        }
    }

    parse_params(&cfg.entry, line);

    /* Groups can use "auto" position of last N files, so we need a counter that changes like this:
     *   #layer of 2         (pos = 0)
     *     #sequence of 2
     *       bgm             pos +1    > 1
     *       bgm             pos +1    > 2
     *     group = -S2       pos -2 +1 > 1 (group is at 1 now since it "collapses" wems but becomes a position)
     *     #sequence of 3
     *       bgm             pos +1    > 2
     *       bgm             pos +1    > 3
     *       #sequence of 2
     *         bgm           pos +1    > 4
     *         bgm           pos +1    > 5
     *       group = -S2     pos -2 +1 > 4 (groups is at 4 now since are uncollapsed wems at 2/3)
     *     group = -S3       pos -3 +1 > 2
     *   group = -L2         pos -2 +1 > 1
     */
    txtp->group_pos++;
    txtp->group_pos -= cfg.count;
    if (auto_pos) {
        cfg.position = txtp->group_pos - 1; /* internally 1 = first */
    }

    //;VGM_LOG("TXTP: parsed group %i%c%i%c, auto=%i\n",cfg.position+1,cfg.type,cfg.count,cfg.repeat, auto_pos);

    /* add final group */
    {
        /* resize in steps if not enough */
        if (txtp->group_count+1 > txtp->group_max) {
            txtp_group *temp_group;

            txtp->group_max += 5;
            temp_group = realloc(txtp->group, sizeof(txtp_group) * txtp->group_max);
            if (!temp_group) goto fail;
            txtp->group = temp_group;
        }

        /* new group */
        txtp->group[txtp->group_count] = cfg; /* memcpy */

        txtp->group_count++;
    }

    return 1;
fail:
    return 0;
}


static void clean_filename(char* filename) {
    int i;
    size_t len;

    if (filename[0] == '\0')
        return;

    /* normalize paths */
    fix_dir_separators(filename);

    /* remove trailing spaces */
    len = strlen(filename);
    for (i = len-1; i > 0; i--) {
        if (filename[i] != ' ')
            break;
        filename[i] = '\0';
    }

}

//TODO see if entry can be set to &default/&entry[entry_count] to avoid add_settings
static int add_entry(txtp_header* txtp, char* filename, int is_default) {
    int i;
    txtp_entry entry = {0};


    //;VGM_LOG("TXTP: filename=%s\n", filename);

    /* parse filename: file.ext#(commands) */
    {
        char* params;

        if (is_default) {
            params = filename; /* multiple commands without filename */
        }
        else {
            /* find settings start after filenames (filenames can also contain dots and #,
             * so this may be fooled by certain patterns) */
            params = strchr(filename, '.'); /* first dot (may be a false positive) */
            if (!params) /* extensionless */
                params = filename;
            params = strchr(params, '#'); /* next should be actual settings */
            if (!params)
                params = NULL;
        }

        parse_params(&entry, params);
    }


    clean_filename(filename);
    //;VGM_LOG("TXTP: clean filename='%s'\n", filename);

    /* settings that applies to final vgmstream */
    if (is_default) {
        txtp->default_entry_set = 1;
        add_settings(&txtp->default_entry, &entry, NULL);
        return 1;
    }

    /* add final entry */
    for (i = entry.range_start; i < entry.range_end; i++){
        txtp_entry* current;

        /* resize in steps if not enough */
        if (txtp->entry_count+1 > txtp->entry_max) {
            txtp_entry* temp_entry;

            txtp->entry_max += 5;
            temp_entry = realloc(txtp->entry, sizeof(txtp_entry) * txtp->entry_max);
            if (!temp_entry) goto fail;
            txtp->entry = temp_entry;
        }

        /* new entry */
        current = &txtp->entry[txtp->entry_count];
        memset(current,0, sizeof(txtp_entry));
        entry.subsong = (i+1);

        add_settings(current, &entry, filename);

        txtp->entry_count++;
        txtp->group_pos++;
    }

    return 1;
fail:
    return 0;
}


/*******************************************************************************/
/* PARSER - BASE                                                               */
/*******************************************************************************/

static int is_substring(const char* val, const char* cmp) {
    int n;
    char subval[TXT_LINE_MAX];

    /* read string without trailing spaces or comments/commands */
    if (sscanf(val, " %s%n[^ #\t\r\n]%n", subval, &n, &n) != 1)
        return 0;

    if (0 != strcmp(subval,cmp))
        return 0;
    return n;
}

static int parse_num(const char* val, uint32_t* out_value) {
    int hex = (val[0]=='0' && val[1]=='x');
    if (sscanf(val, hex ? "%x" : "%u", out_value) != 1)
        goto fail;

    return 1;
fail:
    return 0;
}

static int parse_keyval(txtp_header* txtp, const char* key, const char* val) {
    //;VGM_LOG("TXTP: key=val '%s'='%s'\n", key,val);


    if (0==strcmp(key,"loop_start_segment")) {
        if (!parse_num(val, &txtp->loop_start_segment)) goto fail;
    }
    else if (0==strcmp(key,"loop_end_segment")) {
        if (!parse_num(val, &txtp->loop_end_segment)) goto fail;
    }
    else if (0==strcmp(key,"mode")) {
        if (is_substring(val,"layers")) {
            txtp->is_segmented = 0;
            txtp->is_layered = 1;
        }
        else if (is_substring(val,"segments")) {
            txtp->is_segmented = 1;
            txtp->is_layered = 0;
        }
        else if (is_substring(val,"mixed")) {
            txtp->is_segmented = 0;
            txtp->is_layered = 0;
        }
        else {
            goto fail;
        }
    }
    else if (0==strcmp(key,"loop_mode")) {
        if (is_substring(val,"keep")) {
            txtp->is_loop_keep = 1;
        }
        else if (is_substring(val,"auto")) {
            txtp->is_loop_auto = 1;
        }
        else {
            goto fail;
        }
    }
    else if (0==strcmp(key,"commands")) {
        char val2[TXT_LINE_MAX];
        strcpy(val2, val); /* copy since val is modified here but probably not important */
        if (!add_entry(txtp, val2, 1)) goto fail;
    }
    else if (0==strcmp(key,"group")) {
        char val2[TXT_LINE_MAX];
        strcpy(val2, val); /* copy since val is modified here but probably not important */
        if (!add_group(txtp, val2)) goto fail;

    }
    else {
        goto fail;
    }

    return 1;
fail:
    VGM_LOG("TXTP: error while parsing key=val '%s'='%s'\n", key,val);
    return 0;
}

static txtp_header* parse_txtp(STREAMFILE* sf) {
    txtp_header* txtp = NULL;
    uint32_t txt_offset;


    txtp = calloc(1,sizeof(txtp_header));
    if (!txtp) goto fail;

    /* defaults */
    txtp->is_segmented = 1;

    txt_offset = read_bom(sf);

    /* read and parse lines */
    {
        text_reader_t tr;
        uint8_t buf[TXT_LINE_MAX + 1];
        char key[TXT_LINE_KEY_MAX];
        char val[TXT_LINE_VAL_MAX];
        int ok, line_len;
        char* line;

        if (!text_reader_init(&tr, buf, sizeof(buf), sf, txt_offset, 0))
            goto fail;

        do {
            line_len = text_reader_get_line(&tr, &line);
            if (line_len < 0) goto fail; /* too big for buf (maybe not text)) */

            if (line == NULL) /* EOF */
                break;

            if (line_len == 0) /* empty */
                continue;

            /* try key/val (ignores lead/trail spaces, # may be commands or comments) */
            ok = sscanf(line, " %[^ \t#=] = %[^\t\r\n] ", key,val);
            if (ok == 2) { /* key=val */
                if (!parse_keyval(txtp, key, val)) /* read key/val */
                    goto fail;
                continue;
            }

            /* must be a filename (only remove spaces from start/end, as filenames con contain mid spaces/#/etc) */
            ok = sscanf(line, " %[^\t\r\n] ", val);
            if (ok != 1) /* not a filename either */
                continue;
            if (val[0] == '#')
                continue; /* simple comment */

            /* filename with settings */
            if (!add_entry(txtp, val, 0))
                goto fail;

        } while (line_len >= 0);
    }

    /* mini-txth: if no entries are set try with filename, ex. from "song.ext#3.txtp" use "song.ext#3"
     * (it's possible to have default "commands" inside the .txtp plus filename+settings) */
    if (txtp->entry_count == 0) {
        char filename[PATH_LIMIT];

        filename[0] = '\0';
        get_streamfile_basename(sf, filename, sizeof(filename));

        add_entry(txtp, filename, 0);
    }


    return txtp;
fail:
    clean_txtp(txtp, 1);
    return NULL;
}
