#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../mixing.h"


#define TXTP_LINE_MAX 1024
#define TXTP_MIXING_MAX 512
#define TXTP_GROUP_MODE_SEGMENTED 'S'
#define TXTP_GROUP_MODE_LAYERED 'L'
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
    char filename[TXTP_LINE_MAX];

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

    int trim_set;
    double trim_second;
    int32_t trim_sample;

} txtp_entry;


typedef struct {
    int position;
    char type;
    int count;
    char repeat;

    txtp_entry group_config;

} txtp_group;

typedef struct {
    txtp_entry *entry;
    size_t entry_count;
    size_t entry_max;

    txtp_group *group;
    size_t group_count;
    size_t group_max;

    VGMSTREAM* *vgmstream;
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

static txtp_header* parse_txtp(STREAMFILE* streamFile);
static void clean_txtp(txtp_header* txtp, int fail);
static void apply_config(VGMSTREAM *vgmstream, txtp_entry *current);
void add_mixing(txtp_entry* cfg, txtp_mix_data* mix, txtp_mix_t command);

static int make_group_segment(txtp_header* txtp, int from, int count);
static int make_group_layer(txtp_header* txtp, int from, int count);


/* TXTP - an artificial playlist-like format to play files with segments/layers/config */
VGMSTREAM * init_vgmstream_txtp(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    txtp_header* txtp = NULL;
    int i;


    /* checks */
    if (!check_extensions(streamFile, "txtp"))
        goto fail;

    /* read .txtp with all files and config */
    txtp = parse_txtp(streamFile);
    if (!txtp) goto fail;

    /* post-process */
    {
        if (txtp->entry_count == 0)
            goto fail;

        txtp->vgmstream = calloc(txtp->entry_count, sizeof(VGMSTREAM*));
        if (!txtp->vgmstream) goto fail;

        txtp->vgmstream_count = txtp->entry_count;
    }


    /* detect single files before grouping */
    if (txtp->group_count == 0 && txtp->vgmstream_count == 1) {
        txtp->is_single = 1;
        txtp->is_segmented = 0;
        txtp->is_layered = 0;
    }


    /* open all entry files first as they'll be modified by modes */
    for (i = 0; i < txtp->vgmstream_count; i++) {
        STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[i].filename);
        if (!temp_streamFile) {
            VGM_LOG("TXTP: cannot open streamfile for %s\n", txtp->entry[i].filename);
            goto fail;
        }
        temp_streamFile->stream_index = txtp->entry[i].subsong;

        txtp->vgmstream[i] = init_vgmstream_from_STREAMFILE(temp_streamFile);
        close_streamfile(temp_streamFile);
        if (!txtp->vgmstream[i]) {
            VGM_LOG("TXTP: cannot open vgmstream for %s#%i\n", txtp->entry[i].filename, txtp->entry[i].subsong);
            goto fail;
        }

        apply_config(txtp->vgmstream[i], &txtp->entry[i]);
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
                    if (!make_group_layer(txtp, pos, grp->count))
                        goto fail;
                    break;
                case TXTP_GROUP_MODE_SEGMENTED:
                    if (!make_group_segment(txtp, pos, grp->count))
                        goto fail;
                    break;
                default:
                    goto fail;
            }
        }

        /* group may also have config (like downmixing) */
        apply_config(txtp->vgmstream[grp->position], &grp->group_config);
    }

    /* final tweaks (should be integrated with the above?) */
    if (txtp->is_layered) {
        if (!make_group_layer(txtp, 0, txtp->vgmstream_count))
            goto fail;
    }
    if (txtp->is_segmented) {
        if (!make_group_segment(txtp, 0, txtp->vgmstream_count))
            goto fail;
    }
    if (txtp->is_single) {
        /* special case of setting start_segment to force/overwrite looping
         * (better to use #E but left for compatibility with older TXTPs) */
        if (txtp->loop_start_segment == 1 && !txtp->loop_end_segment) {
            vgmstream_force_loop(txtp->vgmstream[0], 1, txtp->vgmstream[0]->loop_start_sample, txtp->vgmstream[0]->num_samples);
        }
    }


    /* may happen if using mixed mode but some files weren't grouped */
    if (txtp->vgmstream_count != 1) {
        VGM_LOG("TXTP: wrong final vgmstream count %i\n", txtp->vgmstream_count);
        goto fail;
    }

    /* apply default config to the resulting file */
    if (txtp->default_entry_set) {
        apply_config(txtp->vgmstream[0], &txtp->default_entry);
    }


    vgmstream = txtp->vgmstream[0];

    clean_txtp(txtp, 0);
    return vgmstream;

fail:
    clean_txtp(txtp, 1);
    return NULL;
}

static void update_vgmstream_list(VGMSTREAM* vgmstream, txtp_header* txtp, int position, int count) {
    int i;

    //;VGM_LOG("TXTP: compact position=%i count=%i, vgmstreams=%i\n", position, count, txtp->vgmstream_count);

    /* sets and compacts vgmstream list pulling back all following entries */
    txtp->vgmstream[position] = vgmstream;
    for (i = position + count; i < txtp->vgmstream_count; i++) {
        //;VGM_LOG("TXTP: copy %i to %i\n", i, i + 1 - count);
        txtp->vgmstream[i + 1 - count] = txtp->vgmstream[i];
    }

    /* list can only become smaller, no need to alloc/free/etc */
    txtp->vgmstream_count = txtp->vgmstream_count + 1 - count;
    //;VGM_LOG("TXTP: compact vgmstreams=%i\n", txtp->vgmstream_count);
}

static int make_group_segment(txtp_header* txtp, int position, int count) {
    VGMSTREAM * vgmstream = NULL;
    segmented_layout_data *data_s = NULL;
    int i, loop_flag = 0;


    if (count == 1) { /* nothing to do */
        //;VGM_LOG("TXTP: ignored segments of 1\n");
        return 1;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored segment position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return 1;
    }

    /* loop settings only make sense if this group becomes final vgmstream */
    if (position == 0 && txtp->vgmstream_count == count) {
        if (txtp->loop_start_segment && !txtp->loop_end_segment) {
            txtp->loop_end_segment = count;
        }
        else if (txtp->is_loop_auto) { /* auto set to last segment */
            txtp->loop_start_segment = count;
            txtp->loop_end_segment = count;
        }
        loop_flag = (txtp->loop_start_segment > 0 && txtp->loop_start_segment <= count);
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
    vgmstream = allocate_segmented_vgmstream(data_s,loop_flag, txtp->loop_start_segment - 1, txtp->loop_end_segment - 1);
    if (!vgmstream) goto fail;

    /* custom meta name if all parts don't match */
    for (i = 0; i < data_s->segment_count; i++) {
        if (vgmstream->meta_type != data_s->segments[i]->meta_type) {
            vgmstream->meta_type = meta_TXTP;
            break;
        }
    }

    /* fix loop keep */
    if (loop_flag && txtp->is_loop_keep) {
        int32_t current_samples = 0;
        for (i = 0; i < data_s->segment_count; i++) {
            if (txtp->loop_start_segment == i+1 /*&& data_s->segments[i]->loop_start_sample*/) {
                vgmstream->loop_start_sample = current_samples + data_s->segments[i]->loop_start_sample;
            }

            current_samples += data_s->segments[i]->num_samples;

            if (txtp->loop_end_segment == i+1 && data_s->segments[i]->loop_end_sample) {
                vgmstream->loop_end_sample = current_samples - data_s->segments[i]->num_samples + data_s->segments[i]->loop_end_sample;
            }
        }
    }


    /* set new vgmstream and reorder positions */
    update_vgmstream_list(vgmstream, txtp, position, count);

    return 1;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_segmented(data_s);
    return 0;
}

static int make_group_layer(txtp_header* txtp, int position, int count) {
    VGMSTREAM * vgmstream = NULL;
    layered_layout_data * data_l = NULL;
    int i;


    if (count == 1) { /* nothing to do */
        //;VGM_LOG("TXTP: ignored layer of 1\n");
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

    return 1;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data_l);
    return 0;
}


static void apply_config(VGMSTREAM *vgmstream, txtp_entry *current) {

    if (current->config.play_forever) {
        vgmstream->config.play_forever = current->config.play_forever;
    }
    if (current->config.loop_count_set) {
        vgmstream->config.loop_count_set = 1;
        vgmstream->config.loop_count = current->config.loop_count;
    }
    if (current->config.fade_time_set) {
        vgmstream->config.fade_time_set = 1;
        vgmstream->config.fade_time = current->config.fade_time;
    }
    if (current->config.fade_delay_set) {
        vgmstream->config.fade_delay_set = 1;
        vgmstream->config.fade_delay = current->config.fade_delay;
    }
    if (current->config.ignore_fade) {
        vgmstream->config.ignore_fade = current->config.ignore_fade;
    }
    if (current->config.force_loop) {
        vgmstream->config.force_loop = current->config.force_loop;
    }
    if (current->config.really_force_loop) {
        vgmstream->config.really_force_loop = current->config.really_force_loop;
    }
    if (current->config.ignore_loop) {
        vgmstream->config.ignore_loop = current->config.ignore_loop;
    }

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
            current->trim_sample = current->trim_second * vgmstream->sample_rate;
        }

        if (current->trim_sample < 0) {
            vgmstream->num_samples += current->trim_sample; /* trim from end (add negative) */
        }
        else if (vgmstream->num_samples > current->trim_sample) {
            vgmstream->num_samples = current->trim_sample; /* trim to value */
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
}

/* ********************************** */

static void clean_filename(char * filename) {
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

static int get_double(const char * config, double *value, int *is_set) {
    int n, m;
    double temp;

    if (is_set) *is_set = 0;

    m = sscanf(config, " %lf%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    if (is_set) *is_set = 1;
    *value = temp;
    return n;
}

static int get_int(const char * config, int *value) {
    int n,m;
    int temp;

    m = sscanf(config, " %d%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    *value = temp;
    return n;
}

static int get_position(const char * config, double *value_f, char *value_type) {
    int n,m;
    double temp_f;
    char temp_c;

    /* test if format is position: N.n(type) */
    m = sscanf(config, " %lf%c%n", &temp_f,&temp_c,&n);
    if (m != 2 || temp_f < 0.0)
        return 0;
    /* test accepted chars as it will capture anything */
    if (temp_c != TXTP_POSITION_LOOPS)
        return 0;

    *value_f = temp_f;
    *value_type = temp_c;
    return n;
}


static int get_time(const char * config, double *value_f, int32_t *value_i) {
    int n,m;
    int temp_i1, temp_i2;
    double temp_f1, temp_f2;
    char temp_c;

    /* test if format is hour: N:N(.n) or N_N(.n) */
    m = sscanf(config, " %d%c%d%n", &temp_i1,&temp_c,&temp_i2,&n);
    if (m == 3 && (temp_c == ':' || temp_c == '_')) {
        m = sscanf(config, " %lf%c%lf%n", &temp_f1,&temp_c,&temp_f2,&n);
        if (m != 3 || /*temp_f1 < 0.0 ||*/ temp_f1 >= 60.0 || temp_f2 < 0.0 || temp_f2 >= 60.0)
            return 0;

        *value_f = temp_f1 * 60.0 + temp_f2;
        return n;
    }

    /* test if format is seconds: N.n */
    m = sscanf(config, " %d.%d%n", &temp_i1,&temp_i2,&n);
    if (m == 2) {
        m = sscanf(config, " %lf%n", &temp_f1,&n);
        if (m != 1 /*|| temp_f1 < 0.0*/)
            return 0;
        *value_f = temp_f1;
        return n;
    }

    /* test is format is hex samples: 0xN */
    m = sscanf(config, " 0x%x%n", &temp_i1,&n);
    if (m == 1) {
        /* allow negative samples for special meanings */
        //if (temp_i1 < 0)
        //    return 0;

        *value_i = temp_i1;
        return n;
    }

    /* assume format is samples: N */
    m = sscanf(config, " %d%n", &temp_i1,&n);
    if (m == 1) {
        /* allow negative samples for special meanings */
        //if (temp_i1 < 0)
        //    return 0;

        *value_i = temp_i1;
        return n;
    }

    return 0;
}

static int get_bool(const char * config, int *value) {
    int n,m;
    char temp;

    n = 0; /* init as it's not matched if c isn't */
    m = sscanf(config, " %c%n", &temp, &n);
    if (m >= 1 && !(temp == '#' || temp == '\r' || temp == '\n'))
        return 0; /* ignore if anything non-space/comment matched */

    if (m >= 1 && temp == '#')
        n--; /* don't consume separator when returning totals */
    *value = 1;
    return n;
}

static int get_mask(const char * config, uint32_t *value) {
    int n, m, total_n = 0;
    int temp1,temp2, r1, r2;
    int i;
    char cmd;
    uint32_t mask = *value;

    while (config[0] != '\0') {
        m = sscanf(config, " %c%n", &cmd,&n); /* consume comma */
        if (m == 1 && (cmd == ',' || cmd == '-')) { /* '-' is alt separator (space is ok too, implicitly) */
            config += n;
            continue;
        }

        m = sscanf(config, " %d%n ~ %d%n", &temp1,&n, &temp2,&n);
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

        config += n;
        total_n += n;

        if (config[0]== ',' || config[0]== '-')
            config++;
    }

    *value = mask;
    return total_n;
}


static int get_fade(const char * config, txtp_mix_data *mix, int *out_n) {
    int n, m, tn = 0;
    char type, separator;

    m = sscanf(config, " %d %c%n", &mix->ch_dst, &type, &n);
    if (m != 2 || n == 0) goto fail;
    config += n;
    tn += n;

    if (type == '^') {
        /* full definition */
        m = sscanf(config, " %lf ~ %lf = %c @%n", &mix->vol_start, &mix->vol_end, &mix->shape, &n);
        if (m != 3 || n == 0) goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_pre, &mix->sample_pre);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if ( m != 1 || n == 0 || separator != '~') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '+') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_end, &mix->sample_end);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '~') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_post, &mix->sample_post);
        if (n == 0) goto fail;
        config += n;
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

        n = get_position(config, &mix->position, &mix->position_type);
        //if (n == 0) goto fail; /* optional */
        config += n;
        tn += n;

        n = get_time(config, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (m != 1 || n == 0 || separator != '+') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_end, &mix->sample_end);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        mix->time_post = -1.0;
        mix->sample_post = -1;
    }

    mix->time_end = mix->time_start + mix->time_end; /* defined as length */

    *out_n = tn;
    return 1;
fail:
    return 0;
}

void add_mixing(txtp_entry* cfg, txtp_mix_data* mix, txtp_mix_t command) {
    if (cfg->mixing_count + 1 > TXTP_MIXING_MAX) {
        VGM_LOG("TXTP: too many mixes\n");
        return;
    }

    /* parser reads ch1 = first, but for mixing code ch0 = first
     * (if parser reads ch0 here it'll become -1 with meaning of "all channels" in mixing code) */
    mix->ch_dst--;
    mix->ch_src--;
    mix->command = command;

    cfg->mixing[cfg->mixing_count] = *mix; /* memcpy'ed */
    cfg->mixing_count++;
}


static void add_config(txtp_entry* current, txtp_entry* cfg, const char* filename) {

    /* don't memcopy to allow list additions and ignore values not set,
     * as current can be "default" config */
    //*current = *cfg;

    if (filename)
        strcpy(current->filename, filename);

    if (cfg->subsong)
        current->subsong = cfg->subsong;

    if (cfg->channel_mask)
        current->channel_mask = cfg->channel_mask;

    if (cfg->mixing_count > 0) {
        int i;
        for (i = 0; i < cfg->mixing_count; i++) {
            current->mixing[current->mixing_count] = cfg->mixing[i];
            current->mixing_count++;
        }
    }

    if (cfg->config.play_forever) {
        current->config.play_forever = cfg->config.play_forever;
    }
    if (cfg->config.loop_count_set) {
        current->config.loop_count_set = 1;
        current->config.loop_count = cfg->config.loop_count;
    }
    if (cfg->config.fade_time_set) {
        current->config.fade_time_set = 1;
        current->config.fade_time = cfg->config.fade_time;
    }
    if (cfg->config.fade_delay_set) {
        current->config.fade_delay_set = 1;
        current->config.fade_delay = cfg->config.fade_delay;
    }
    if (cfg->config.ignore_fade) {
        current->config.ignore_fade = cfg->config.ignore_fade;
    }
    if (cfg->config.force_loop) {
        current->config.force_loop = cfg->config.force_loop;
    }
    if (cfg->config.really_force_loop) {
        current->config.really_force_loop = cfg->config.really_force_loop;
    }
    if (cfg->config.ignore_loop) {
        current->config.ignore_loop = cfg->config.ignore_loop;
    }

    if (cfg->sample_rate > 0) {
        current->sample_rate = cfg->sample_rate;
    }

    if (cfg->loop_install_set) {
        current->loop_install_set = cfg->loop_install_set;
        current->loop_end_max = cfg->loop_end_max;
        current->loop_start_sample = cfg->loop_start_sample;
        current->loop_start_second = cfg->loop_start_second;
        current->loop_end_sample = cfg->loop_end_sample;
        current->loop_end_second = cfg->loop_end_second;
    }

    if (cfg->trim_set) {
        current->trim_set = cfg->trim_set;
        current->trim_second = cfg->trim_second;
        current->trim_sample = cfg->trim_sample;
    }
}

static void parse_config(txtp_entry *cfg, char *config) {
    /* parse config: #(commands) */
    int n, nc, nm, mc;
    char command[TXTP_LINE_MAX] = {0};

    cfg->range_start = 0;
    cfg->range_end = 1;

    while (config != NULL) {
        /* position in next #(command) */
        config = strchr(config, '#');
        if (!config) break;
        //;VGM_LOG("TXTP: config='%s'\n", config);

        /* get command until next space/number/comment/end */
        command[0] = '\0';
        mc = sscanf(config, "#%n%[^ #0-9\r\n]%n", &nc, command, &nc);
        //;VGM_LOG("TXTP:  command='%s', nc=%i, mc=%i\n", command, nc, mc);
        if (mc <= 0 && nc == 0) break;

        config[0] = '\0'; //todo don't modify input string and properly calculate filename end

        config += nc; /* skip '#' and command */

        /* check command string (though at the moment we only use single letters) */
        if (strcmp(command,"c") == 0) {
            /* channel mask: file.ext#c1,2 = play channels 1,2 and mutes rest */

            config += get_mask(config, &cfg->channel_mask);
            //;VGM_LOG("TXTP:   channel_mask ");{int i; for (i=0;i<16;i++)VGM_LOG("%i ",(cfg->channel_mask>>i)&1);}VGM_LOG("\n");
        }
        else if (strcmp(command,"m") == 0) {
            /* channel mixing: file.ext#m(sub-command),(sub-command),etc */
            char cmd;

            while (config[0] != '\0') {
                txtp_mix_data mix = {0};

                //;VGM_LOG("TXTP: subcommand='%s'\n", config);

                //todo use strchr instead?
                if (sscanf(config, " %c%n", &cmd, &n) == 1 && n != 0 && cmd == ',') {
                    config += n;
                    continue;
                }

                if (sscanf(config, " %d - %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i-%i\n", mix.ch_dst, mix.ch_src);
                    add_mixing(cfg, &mix, MIX_SWAP); /* N-M: swaps M with N */
                    config += n;
                    continue;
                }

                if ((sscanf(config, " %d + %d * %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0) ||
                    (sscanf(config, " %d + %d x %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i+%i*%f\n", mix.ch_dst, mix.ch_src, mix.vol);
                    add_mixing(cfg, &mix, MIX_ADD_VOLUME); /* N+M*V: mixes M*volume to N */
                    config += n;
                    continue;
                }

                if (sscanf(config, " %d + %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i+%i\n", mix.ch_dst, mix.ch_src);
                    add_mixing(cfg, &mix, MIX_ADD); /* N+M: mixes M to N */
                    config += n;
                    continue;
                }

                if ((sscanf(config, " %d * %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0) ||
                    (sscanf(config, " %d x %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i*%f\n", mix.ch_dst, mix.vol);
                    add_mixing(cfg, &mix, MIX_VOLUME); /* N*V: changes volume of N */
                    config += n;
                    continue;
                }

                if ((sscanf(config, " %d = %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i=%f\n", mix.ch_dst, mix.vol);
                    add_mixing(cfg, &mix, MIX_LIMIT); /* N=V: limits volume of N */
                    config += n;
                    continue;
                }

                if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'D') {
                    //;VGM_LOG("TXTP:   mix %iD\n", mix.ch_dst);
                    add_mixing(cfg, &mix, MIX_KILLMIX); /* ND: downmix N and all following channels */
                    config += n;
                    continue;
                }

                if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'd') {
                    //;VGM_LOG("TXTP:   mix %id\n", mix.ch_dst);
                    add_mixing(cfg, &mix, MIX_DOWNMIX);/* Nd: downmix N only */
                    config += n;
                    continue;
                }

                if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'u') {
                    //;VGM_LOG("TXTP:   mix %iu\n", mix.ch_dst);
                    add_mixing(cfg, &mix, MIX_UPMIX); /* Nu: upmix N */
                    config += n;
                    continue;
                }

                if (get_fade(config, &mix, &n) != 0) {
                    //;VGM_LOG("TXTP:   fade %d^%f~%f=%c@%f~%f+%f~%f\n",
                    //        mix.ch_dst, mix.vol_start, mix.vol_end, mix.shape,
                    //        mix.time_pre, mix.time_start, mix.time_end, mix.time_post);
                    add_mixing(cfg, &mix, MIX_FADE); /* N^V1~V2@T1~T2+T3~T4: fades volumes between positions */
                    config += n;
                    continue;
                }

                break; /* unknown mix/new command/end */
           }
        }
        else if (strcmp(command,"s") == 0 || (nc == 1 && config[0] >= '0' && config[0] <= '9')) {
            /* subsongs: file.ext#s2 = play subsong 2, file.ext#2~10 = play subsong range */
            int subsong_start = 0, subsong_end = 0;

            //todo also advance config?
            if (sscanf(config, " %d ~ %d", &subsong_start, &subsong_end) == 2) {
                if (subsong_start > 0 && subsong_end > 0) {
                    cfg->range_start = subsong_start-1;
                    cfg->range_end = subsong_end;
                }
                //;VGM_LOG("TXTP:   subsong range %i~%i\n", range_start, range_end);
            }
            else if (sscanf(config, " %d", &subsong_start) == 1) {
                if (subsong_start > 0) {
                    cfg->range_start = subsong_start-1;
                    cfg->range_end = subsong_start;
                }
                //;VGM_LOG("TXTP:   subsong single %i-%i\n", range_start, range_end);
            }
            else { /* wrong config, ignore */
                //;VGM_LOG("TXTP:   subsong none\n");
            }
        }
        else if (strcmp(command,"i") == 0) {
            config += get_bool(config, &cfg->config.ignore_loop);
            //;VGM_LOG("TXTP:   ignore_loop=%i\n", cfg->config.ignore_loop);
        }
        else if (strcmp(command,"e") == 0) {
            config += get_bool(config, &cfg->config.force_loop);
            //;VGM_LOG("TXTP:   force_loop=%i\n", cfg->config.force_loop);
        }
        else if (strcmp(command,"E") == 0) {
            config += get_bool(config, &cfg->config.really_force_loop);
            //;VGM_LOG("TXTP:   really_force_loop=%i\n", cfg->config.really_force_loop);
        }
        else if (strcmp(command,"F") == 0) {
            config += get_bool(config, &cfg->config.ignore_fade);
            //;VGM_LOG("TXTP:   ignore_fade=%i\n", cfg->config.ignore_fade);
        }
        else if (strcmp(command,"L") == 0) {
            config += get_bool(config, &cfg->config.play_forever);
            //;VGM_LOG("TXTP:   play_forever=%i\n", cfg->config.play_forever);
        }
        else if (strcmp(command,"l") == 0) {
            config += get_double(config, &cfg->config.loop_count, &cfg->config.loop_count_set);
            if (cfg->config.loop_count < 0)
                cfg->config.loop_count_set = 0;
            //;VGM_LOG("TXTP:   loop_count=%f\n", cfg->config.loop_count);
        }
        else if (strcmp(command,"f") == 0) {
            config += get_double(config, &cfg->config.fade_time, &cfg->config.fade_time_set);
            if (cfg->config.fade_time < 0)
                cfg->config.fade_time_set = 0;
            //;VGM_LOG("TXTP:   fade_time=%f\n", cfg->config.fade_time);
        }
        else if (strcmp(command,"d") == 0) {
            config += get_double(config, &cfg->config.fade_delay, &cfg->config.fade_delay_set);
            if (cfg->config.fade_delay < 0)
                cfg->config.fade_delay_set = 0;
            //;VGM_LOG("TXTP:   fade_delay %f\n", cfg->config.fade_delay);
        }
        else if (strcmp(command,"h") == 0) {
            config += get_int(config, &cfg->sample_rate);
            //;VGM_LOG("TXTP:   sample_rate %i\n", cfg->sample_rate);
        }
        else if (strcmp(command,"I") == 0) {
            n = get_time(config,  &cfg->loop_start_second, &cfg->loop_start_sample);
            if (n > 0) { /* first value must exist */
                config += n;

                n = get_time(config,  &cfg->loop_end_second, &cfg->loop_end_sample);
                if (n == 0) { /* second value is optional */
                    cfg->loop_end_max = 1;
                }

                config += n;
                cfg->loop_install_set = 1;
            }

            //;VGM_LOG("TXTP:   loop_install %i (max=%i): %i %i / %f %f\n", cfg->loop_install, cfg->loop_end_max,
            //        cfg->loop_start_sample, cfg->loop_end_sample, cfg->loop_start_second, cfg->loop_end_second);
        }
        else if (strcmp(command,"t") == 0) {
            n = get_time(config,  &cfg->trim_second, &cfg->trim_sample);
            cfg->trim_set = (n > 0);
            //;VGM_LOG("TXTP: trim %i - %f / %i\n", cfg->trim_set, cfg->trim_second, cfg->trim_sample);
        }
        //todo cleanup
        else if (strcmp(command,"@volume") == 0) {
            txtp_mix_data mix = {0};

            nm = get_double(config, &mix.vol, NULL);
            config += nm;

            if (nm == 0) continue;

            nm = get_mask(config, &mix.mask);
            config += nm;

            add_mixing(cfg, &mix, MACRO_VOLUME);
        }
        else if (strcmp(command,"@track") == 0 ||
                 strcmp(command,"C") == 0 ) {
            txtp_mix_data mix = {0};

            nm = get_mask(config, &mix.mask);
            config += nm;
            if (nm == 0) continue;

            add_mixing(cfg, &mix, MACRO_TRACK);
        }
        else if (strcmp(command,"@layer-v") == 0 ||
                 strcmp(command,"@layer-b") == 0 ||
                 strcmp(command,"@layer-e") == 0) {
            txtp_mix_data mix = {0};

            nm = get_int(config, &mix.max);
            config += nm;
            if (nm == 0) continue;

            nm = get_mask(config, &mix.mask);
            config += nm;

            mix.mode = command[7]; /* pass letter */
            add_mixing(cfg, &mix, MACRO_LAYER);
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

            nm = get_int(config, &mix.max);
            config += nm;
            if (nm == 0) continue;

            add_mixing(cfg, &mix, type);
        }
        else if (strcmp(command,"@downmix") == 0) {
            txtp_mix_data mix = {0};

            mix.max = 2; /* stereo only for now */
            //nm = get_int(config, &mix.max);
            //config += nm;
            //if (nm == 0) continue;

            add_mixing(cfg, &mix, MACRO_DOWNMIX);
        }
        else if (config[nc] == ' ') {
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



static int add_group(txtp_header * txtp, char *line) {
    int n, m;
    txtp_group cfg = {0};

    /* parse group: (position)(type)(count)(repeat)  #(commands) */
    //;VGM_LOG("TXTP: parse group '%s'\n", line);

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
        line += n;
    }


    parse_config(&cfg.group_config, line);

    //;VGM_LOG("TXTP: parsed group %i%c%i%c\n",cfg.position+1,cfg.type,cfg.count,cfg.repeat);

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


static int add_entry(txtp_header * txtp, char *filename, int is_default) {
    int i;
    txtp_entry cfg = {0};


    //;VGM_LOG("TXTP: filename=%s\n", filename);

    /* parse filename: file.ext#(commands) */
    {
        char *config;

        if (is_default) {
            config = filename; /* multiple commands without filename */
        }
        else {
            /* find config start (filenames and config can contain multiple dots and #,
             * so this may be fooled by certain patterns of . and #) */
            config = strchr(filename, '.'); /* first dot (may be a false positive) */
            if (!config) /* extensionless */
                config = filename;
            config = strchr(config, '#'); /* next should be config */
            if (!config) /* no config */
                config = NULL;
        }

        parse_config(&cfg, config);
    }


    clean_filename(filename);
    //;VGM_LOG("TXTP: clean filename='%s'\n", filename);

    /* config that applies to all files */
    if (is_default) {
        txtp->default_entry_set = 1;
        add_config(&txtp->default_entry, &cfg, NULL);
        return 1;
    }

    /* add final entry */
    for (i = cfg.range_start; i < cfg.range_end; i++){
        txtp_entry *current;

        /* resize in steps if not enough */
        if (txtp->entry_count+1 > txtp->entry_max) {
            txtp_entry *temp_entry;

            txtp->entry_max += 5;
            temp_entry = realloc(txtp->entry, sizeof(txtp_entry) * txtp->entry_max);
            if (!temp_entry) goto fail;
            txtp->entry = temp_entry;
        }

        /* new entry */
        current = &txtp->entry[txtp->entry_count];
        memset(current,0, sizeof(txtp_entry));
        cfg.subsong = (i+1);

        add_config(current, &cfg, filename);

        txtp->entry_count++;
    }

    return 1;
fail:
    return 0;
}

/* ************************************************************************ */

static int is_substring(const char * val, const char * cmp) {
    int n;
    char subval[TXTP_LINE_MAX] = {0};

    /* read string without trailing spaces or comments/commands */
    if (sscanf(val, " %s%n[^ #\t\r\n]%n", subval, &n, &n) != 1)
        return 0;

    if (0 != strcmp(subval,cmp))
        return 0;
    return n;
}

static int parse_num(const char * val, uint32_t * out_value) {
    int hex = (val[0]=='0' && val[1]=='x');
    if (sscanf(val, hex ? "%x" : "%u", out_value) != 1)
        goto fail;

    return 1;
fail:
    return 0;
}

static int parse_keyval(txtp_header * txtp, const char * key, const char * val) {
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
        char val2[TXTP_LINE_MAX];
        strcpy(val2, val); /* copy since val is modified here but probably not important */
        if (!add_entry(txtp, val2, 1)) goto fail;
    }
    else if (0==strcmp(key,"group")) {
        char val2[TXTP_LINE_MAX];
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

static txtp_header* parse_txtp(STREAMFILE* streamFile) {
    txtp_header* txtp = NULL;
    off_t txt_offset = 0x00;
    off_t file_size = get_streamfile_size(streamFile);


    txtp = calloc(1,sizeof(txtp_header));
    if (!txtp) goto fail;

    /* defaults */
    txtp->is_segmented = 1;


    /* skip BOM if needed */
    if (file_size > 0 &&
            ((uint16_t)read_16bitLE(0x00, streamFile) == 0xFFFE || (uint16_t)read_16bitLE(0x00, streamFile) == 0xFEFF))
        txt_offset = 0x02;

    /* read and parse lines */
    while (txt_offset < file_size) {
        char line[TXTP_LINE_MAX];
        char key[TXTP_LINE_MAX] = {0}, val[TXTP_LINE_MAX] = {0}; /* at least as big as a line to avoid overflows (I hope) */
        char filename[TXTP_LINE_MAX] = {0};
        int ok, bytes_read, line_ok;

        bytes_read = read_line(line, sizeof(line), txt_offset, streamFile, &line_ok);
        if (!line_ok) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trail spaces, # may be commands or comments) */
        ok = sscanf(line, " %[^ \t#=] = %[^\t\r\n] ", key,val);
        if (ok == 2) { /* key=val */
            if (!parse_keyval(txtp, key, val)) /* read key/val */
                goto fail;
            continue;
        }

        /* must be a filename (only remove spaces from start/end, as filenames con contain mid spaces/#/etc) */
        ok = sscanf(line, " %[^\t\r\n] ", filename);
        if (ok != 1) /* not a filename either */
            continue;
        if (filename[0] == '#')
            continue; /* simple comment */

        /* filename with config */
        if (!add_entry(txtp, filename, 0))
            goto fail;
    }

    /* mini-txth: if no entries are set try with filename, ex. from "song.ext#3.txtp" use "song.ext#3"
     * (it's possible to have default "commands" inside the .txtp plus filename+config) */
    if (txtp->entry_count == 0) {
        char filename[PATH_LIMIT] = {0};

        get_streamfile_basename(streamFile, filename, sizeof(filename));

        add_entry(txtp, filename, 0);
    }


    return txtp;
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
