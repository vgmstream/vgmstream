#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#ifdef VGMSTREAM_MIXING
#include "../mixing.h"
#endif


#define TXTP_LINE_MAX 1024
#define TXTP_MIXING_MAX 128

#ifdef VGMSTREAM_MIXING
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

    /* macros */
    int max;
    uint32_t mask;
    char mode;
} txtp_mix_data;
#endif


typedef struct {
    char filename[TXTP_LINE_MAX];
    int subsong;
    uint32_t channel_mask;
#ifndef VGMSTREAM_MIXING
    int channel_mappings_on;
    int channel_mappings[32];
#endif
#ifdef VGMSTREAM_MIXING
    int mixing_count;
    txtp_mix_data mixing[TXTP_MIXING_MAX];
#endif

    double config_loop_count;
    double config_fade_time;
    double config_fade_delay;
    int config_ignore_loop;
    int config_force_loop;
    int config_ignore_fade;

    int sample_rate;
    int loop_install;
    int loop_end_max;
    double loop_start_second;
    int32_t loop_start_sample;
    double loop_end_second;
    int32_t loop_end_sample;

} txtp_entry;

typedef struct {
    txtp_entry *entry;
    size_t entry_count;
    size_t entry_max;

    uint32_t loop_start_segment;
    uint32_t loop_end_segment;

    txtp_entry default_entry;
    int default_entry_set;

    size_t is_layered;
    int is_loop_keep;
} txtp_header;

static txtp_header* parse_txtp(STREAMFILE* streamFile);
static void clean_txtp(txtp_header* txtp);
static void apply_config(VGMSTREAM *vgmstream, txtp_entry *current);
#ifdef VGMSTREAM_MIXING
void add_mixing(txtp_entry* cfg, txtp_mix_data* mix, txtp_mix_t command);
#endif

/* TXTP - an artificial playlist-like format to play files with segments/layers/config */
VGMSTREAM * init_vgmstream_txtp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    txtp_header* txtp = NULL;
    segmented_layout_data *data_s = NULL;
    layered_layout_data * data_l = NULL;
    int i;


    /* checks */
    if (!check_extensions(streamFile, "txtp"))
        goto fail;

    /* read .txtp text file to get segments */
    txtp = parse_txtp(streamFile);
    if (!txtp) goto fail;


    if (txtp->entry_count == 0)
        goto fail;


    if (txtp->entry_count == 1 && !txtp->loop_start_segment) {
        /* single file */
        STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[0].filename);
        if (!temp_streamFile) goto fail;
        temp_streamFile->stream_index = txtp->entry[0].subsong;

        vgmstream = init_vgmstream_from_STREAMFILE(temp_streamFile);
        close_streamfile(temp_streamFile);
        if (!vgmstream) goto fail;

        apply_config(vgmstream, &txtp->entry[0]);
    }
    else if (txtp->is_layered) {
        /* layered multi file */

        /* init layout */
        data_l = init_layout_layered(txtp->entry_count);
        if (!data_l) goto fail;

        /* open each layer subfile */
        for (i = 0; i < data_l->layer_count; i++) {
            STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[i].filename);
            if (!temp_streamFile) goto fail;
            temp_streamFile->stream_index = txtp->entry[i].subsong;

            data_l->layers[i] = init_vgmstream_from_STREAMFILE(temp_streamFile);
            close_streamfile(temp_streamFile);
            if (!data_l->layers[i]) goto fail;

            apply_config(data_l->layers[i], &txtp->entry[i]);
        }

        /* setup layered VGMSTREAMs */
        if (!setup_layout_layered(data_l))
            goto fail;

        /* build the layered VGMSTREAM */
        vgmstream = allocate_layered_vgmstream(data_l);
        if (!vgmstream) goto fail;

        /* custom meta name if all parts don't match */
        for (i = 0; i < data_l->layer_count; i++) {
            if (vgmstream->meta_type != data_l->layers[i]->meta_type) {
                vgmstream->meta_type = meta_TXTP;
                break;
            }
        }
    }
    else {
        /* segmented multi file */
        int loop_flag;

        /* init layout */
        data_s = init_layout_segmented(txtp->entry_count);
        if (!data_s) goto fail;

        /* open each segment subfile */
        for (i = 0; i < data_s->segment_count; i++) {
            STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[i].filename);
            if (!temp_streamFile) goto fail;

            /* subsongs ranges also work for files without subsongs (as to repeat the same file), not sure if bug or feature */
            temp_streamFile->stream_index = txtp->entry[i].subsong;

            data_s->segments[i] = init_vgmstream_from_STREAMFILE(temp_streamFile);
            close_streamfile(temp_streamFile);
            if (!data_s->segments[i]) goto fail;

            apply_config(data_s->segments[i], &txtp->entry[i]);
        }

        /* setup segmented VGMSTREAMs */
        if (!setup_layout_segmented(data_s))
            goto fail;

        /* get looping and samples */
        if (txtp->loop_start_segment && !txtp->loop_end_segment)
            txtp->loop_end_segment = txtp->entry_count;
        loop_flag = (txtp->loop_start_segment > 0 && txtp->loop_start_segment <= txtp->entry_count);


        /* build the VGMSTREAM */
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
    }


    /* apply default config to the resulting file */
    if (txtp->default_entry_set) {
        apply_config(vgmstream, &txtp->default_entry);
    }


    clean_txtp(txtp);
    return vgmstream;

fail:
    clean_txtp(txtp);
    close_vgmstream(vgmstream);
    if (!vgmstream) {
        free_layout_segmented(data_s);
        free_layout_layered(data_l);
    }
    return NULL;
}

static void apply_config(VGMSTREAM *vgmstream, txtp_entry *current) {
#ifndef VGMSTREAM_MIXING
    vgmstream->channel_mask = current->channel_mask;

    vgmstream->channel_mappings_on = current->channel_mappings_on;
    if (vgmstream->channel_mappings_on) {
        int ch;
        for (ch = 0; ch < 32; ch++) {
            vgmstream->channel_mappings[ch] = current->channel_mappings[ch];
        }
    }
#endif

    vgmstream->config_loop_count = current->config_loop_count;
    vgmstream->config_fade_time = current->config_fade_time;
    vgmstream->config_fade_delay = current->config_fade_delay;
    vgmstream->config_ignore_loop = current->config_ignore_loop;
    vgmstream->config_force_loop = current->config_force_loop;
    vgmstream->config_ignore_fade = current->config_ignore_fade;

    if (current->sample_rate > 0) {
        vgmstream->sample_rate = current->sample_rate;
    }

    if (current->loop_install) {
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

        vgmstream_force_loop(vgmstream, current->loop_install, current->loop_start_sample, current->loop_end_sample);
    }

#ifdef VGMSTREAM_MIXING
    /* add macro to mixing list */
    if (current->channel_mask) {
        int ch;
        for (ch = 0; ch < vgmstream->channels; ch++) {
            if (!((current->channel_mask >> ch) & 1)) {
                txtp_mix_data mix = {0};
                mix.ch_dst = ch;
                mix.vol = 0.0f;
                add_mixing(current, &mix, MIX_VOLUME);
            }
        }
    }

    /* copy mixing list (should be done last as some mixes depend on config) */
    if (current->mixing_count > 0) {
        int m;

        for (m = 0; m < current->mixing_count; m++) {
            txtp_mix_data mix = current->mixing[m];

            switch(mix.command) {
                /* base mixes */
                case MIX_SWAP:       mixing_push_swap(vgmstream, mix.ch_dst, mix.ch_src); break;
                case MIX_ADD:        mixing_push_add(vgmstream, mix.ch_dst, mix.ch_src, 1.0); break;
                case MIX_ADD_VOLUME: mixing_push_add(vgmstream, mix.ch_dst, mix.ch_src, mix.vol); break;
                case MIX_VOLUME:     mixing_push_volume(vgmstream, mix.ch_dst, mix.vol); break;
                case MIX_LIMIT:      mixing_push_limit(vgmstream, mix.ch_dst, mix.vol); break;
                case MIX_UPMIX:      mixing_push_upmix(vgmstream, mix.ch_dst); break;
                case MIX_DOWNMIX:    mixing_push_downmix(vgmstream, mix.ch_dst); break;
                case MIX_KILLMIX:    mixing_push_killmix(vgmstream, mix.ch_dst); break;
                case MIX_FADE:
                    /* Convert from time to samples now that sample rate is final.
                     * Samples and time values may be mixed though, so it's done for every
                     * value (if one is 0 the other will be too, though) */
                    if (mix.time_pre > 0.0)   mix.sample_pre = mix.time_pre * vgmstream->sample_rate;
                    if (mix.time_start > 0.0) mix.sample_start = mix.time_start * vgmstream->sample_rate;
                    if (mix.time_end > 0.0)   mix.sample_end = mix.time_end * vgmstream->sample_rate;
                    if (mix.time_post > 0.0)  mix.sample_post = mix.time_post * vgmstream->sample_rate;
                    /* convert special meaning too */
                    if (mix.time_pre < 0.0)   mix.sample_pre = -1;
                    if (mix.time_post < 0.0)  mix.sample_post = -1;

                    mixing_push_fade(vgmstream, mix.ch_dst, mix.vol_start, mix.vol_end, mix.shape,
                            mix.sample_pre, mix.sample_start, mix.sample_end, mix.sample_post);
                    break;

                /* macro mixes */
                case MACRO_VOLUME:      mixing_macro_volume(vgmstream, mix.vol, mix.mask); break;
                case MACRO_TRACK:       mixing_macro_track(vgmstream, mix.mask); break;
                case MACRO_LAYER:       mixing_macro_layer(vgmstream, mix.max, mix.mask, mix.mode); break;
                case MACRO_CROSSTRACK:  mixing_macro_crosstrack(vgmstream, mix.max); break;
                case MACRO_CROSSLAYER:  mixing_macro_crosslayer(vgmstream, mix.max, mix.mode); break;

                default:
                    break;
            }
        }
    }
#endif
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

static int get_double(const char * config, double *value) {
    int n, m;
    double temp;

    m = sscanf(config, " %lf%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    *value = temp;
    return n;
}

static int get_int(const char * config, int *value) {
    int n,m;
    int temp;

    m = sscanf(config, " %i%n", &temp,&n);
    if (m != 1 || temp < 0)
        return 0;

    *value = temp;
    return n;
}

static int get_time(const char * config, double *value_f, int32_t *value_i) {
    int n,m;
    int temp_i1, temp_i2;
    double temp_f1, temp_f2;
    char temp_c;

    /* test if format is hour: N:N(.n) or N_N(.n) */
    m = sscanf(config, " %i%c%i%n", &temp_i1,&temp_c,&temp_i2,&n);
    if (m == 3 && (temp_c == ':' || temp_c == '_')) {
        m = sscanf(config, " %lf%c%lf%n", &temp_f1,&temp_c,&temp_f2,&n);
        if (m != 3 || temp_f1 < 0.0 || temp_f1 >= 60.0 || temp_f2 < 0.0 || temp_f2 >= 60.0)
            return 0;

        *value_f = temp_f1 * 60.0 + temp_f2;
        return n;
    }

    /* test if format is seconds: N.n */
    m = sscanf(config, " %i.%i%n", &temp_i1,&temp_i2,&n);
    if (m == 2) {
        m = sscanf(config, " %lf%n", &temp_f1,&n);
        if (m != 1 || temp_f1 < 0.0)
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
    m = sscanf(config, " %i%n", &temp_i1,&n);
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


#ifdef VGMSTREAM_MIXING
static int get_fade(const char * config, txtp_mix_data *mix, int *out_n) {
    int n, m, tn = 0;
    char type, separator;

    m = sscanf(config, " %d %c%n", &mix->ch_dst, &type, &n);
    if (n == 0 || m != 2) goto fail;
    config += n;
    tn += n;

    if (type == '^') {
        /* full definition */
        m = sscanf(config, " %lf ~ %lf = %c @%n", &mix->vol_start, &mix->vol_end, &mix->shape, &n);
        if (n == 0 || m != 3) goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_pre, &mix->sample_pre);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (n == 0 || m != 1 || separator != '~') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (n == 0 || m != 1 || separator != '+') goto fail;
        config += n;
        tn += n;

        n = get_time(config, &mix->time_end, &mix->sample_end);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (n == 0 || m != 1 || separator != '~') goto fail;
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

        n = get_time(config, &mix->time_start, &mix->sample_start);
        if (n == 0) goto fail;
        config += n;
        tn += n;

        m = sscanf(config, " %c%n", &separator, &n);
        if (n == 0 || m != 1 || separator != '+') goto fail;
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
#endif

#ifdef VGMSTREAM_MIXING
void add_mixing(txtp_entry* cfg, txtp_mix_data* mix, txtp_mix_t command) {
    if (cfg->mixing_count + 1 > TXTP_MIXING_MAX) {
        VGM_LOG("TXTP: too many mixes\n");
        return;
    }

    /* parsers reads ch1 = first, but for mixing code ch0 = first
     * (if parser reads ch0 here it'll become -1 with special meaning in code) */
    mix->ch_dst--;
    mix->ch_src--;
    mix->command = command;

    cfg->mixing[cfg->mixing_count] = *mix; /* memcpy'ed */
    cfg->mixing_count++;
}
#endif


static void add_config(txtp_entry* current, txtp_entry* cfg, const char* filename) {
    strcpy(current->filename, filename);

    current->subsong = cfg->subsong;

    current->channel_mask = cfg->channel_mask;

#ifndef VGMSTREAM_MIXING
    if (cfg->channel_mappings_on) {
        int ch;
        current->channel_mappings_on = cfg->channel_mappings_on;
        for (ch = 0; ch < 32; ch++) {
            current->channel_mappings[ch] = cfg->channel_mappings[ch];
        }
    }
#endif
#ifdef VGMSTREAM_MIXING
    //*current = *cfg; /* don't memcopy to allow list additions */ //todo save list first then memcpy

    if (cfg->mixing_count > 0) {
        int i;
        for (i = 0; i < cfg->mixing_count; i++) {
            current->mixing[current->mixing_count] = cfg->mixing[i];
            current->mixing_count++;
        }
    }
#endif

    current->config_loop_count = cfg->config_loop_count;
    current->config_fade_time = cfg->config_fade_time;
    current->config_fade_delay = cfg->config_fade_delay;
    current->config_ignore_loop = cfg->config_ignore_loop;
    current->config_force_loop = cfg->config_force_loop;
    current->config_ignore_fade = cfg->config_ignore_fade;

    current->sample_rate = cfg->sample_rate;
    current->loop_install = cfg->loop_install;
    current->loop_end_max = cfg->loop_end_max;
    current->loop_start_sample = cfg->loop_start_sample;
    current->loop_start_second = cfg->loop_start_second;
    current->loop_end_sample = cfg->loop_end_sample;
    current->loop_end_second = cfg->loop_end_second;
}

static int add_filename(txtp_header * txtp, char *filename, int is_default) {
    int i, n, nc, nm, mc;
    txtp_entry cfg = {0};
    size_t range_start, range_end;
    char command[TXTP_LINE_MAX] = {0};


    //;VGM_LOG("TXTP: filename=%s\n", filename);

    /* parse config: file.ext#(commands) */
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
                config = filename; //todo if no config just exit?
        }

        range_start = 0;
        range_end = 1;

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

                config += get_mask(config, &cfg.channel_mask);
                //;VGM_LOG("TXTP:   channel_mask ");{int i; for (i=0;i<16;i++)VGM_LOG("%i ",(cfg.channel_mask>>i)&1);}VGM_LOG("\n");
            }
#ifndef VGMSTREAM_MIXING
            else if (strcmp(command,"m") == 0) {
                /* channel mappings: file.ext#m1-2,3-4 = swaps channels 1<>2 and 3<>4 */
                int ch_from = 0, ch_to = 0;

                cfg.channel_mappings_on = 1;
                while (config[0] != '\0') {
                    if (sscanf(config, " %d%n", &ch_from, &n) != 1)
                        break;
                    config += n;
                    if (config[0]== ',' || config[0]== '-')
                        config++;

                    if (sscanf(config, " %d%n", &ch_to, &n) != 1)
                        break;
                    config += n;
                    if (config[0]== ',' || config[0]== '-')
                        config++;

                    if (ch_from > 0 && ch_from <= 32 && ch_to > 0 && ch_to <= 32) {
                        cfg.channel_mappings[ch_from-1] = ch_to-1;
                    }
                    //;VGM_LOG("TXTP:   channel_swap %i-%i\n", ch_from, ch_to);
               }
            }
#endif
#ifdef VGMSTREAM_MIXING
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
                        add_mixing(&cfg, &mix, MIX_SWAP); /* N-M: swaps M with N */
                        config += n;
                        continue;
                    }

                    if ((sscanf(config, " %d + %d * %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0) ||
                        (sscanf(config, " %d + %d x %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0)) {
                        //;VGM_LOG("TXTP:   mix %i+%i*%f\n", mix.ch_dst, mix.ch_src, mix.vol);
                        add_mixing(&cfg, &mix, MIX_ADD_VOLUME); /* N+M*V: mixes M*volume to N */
                        config += n;
                        continue;
                    }

                    if (sscanf(config, " %d + %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                        //;VGM_LOG("TXTP:   mix %i+%i\n", mix.ch_dst, mix.ch_src);
                        add_mixing(&cfg, &mix, MIX_ADD); /* N+M: mixes M to N */
                        config += n;
                        continue;
                    }

                    if ((sscanf(config, " %d * %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0) ||
                        (sscanf(config, " %d x %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                        //;VGM_LOG("TXTP:   mix %i*%f\n", mix.ch_dst, mix.vol);
                        add_mixing(&cfg, &mix, MIX_VOLUME); /* N*V: changes volume of N */
                        config += n;
                        continue;
                    }

                    if ((sscanf(config, " %d = %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                        //;VGM_LOG("TXTP:   mix %i=%f\n", mix.ch_dst, mix.vol);
                        add_mixing(&cfg, &mix, MIX_LIMIT); /* N=V: limits volume of N */
                        config += n;
                        continue;
                    }

                    if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'D') {
                        //;VGM_LOG("TXTP:   mix %iD\n", mix.ch_dst);
                        add_mixing(&cfg, &mix, MIX_KILLMIX); /* ND: downmix N and all following channels */
                        config += n;
                        continue;
                    }

                    if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'd') {
                        //;VGM_LOG("TXTP:   mix %id\n", mix.ch_dst);
                        add_mixing(&cfg, &mix, MIX_DOWNMIX);/* Nd: downmix N only */
                        config += n;
                        continue;
                    }

                    if (sscanf(config, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'u') {
                        //;VGM_LOG("TXTP:   mix %iu\n", mix.ch_dst);
                        add_mixing(&cfg, &mix, MIX_UPMIX); /* Nu: upmix N */
                        config += n;
                        continue;
                    }

                    if (get_fade(config, &mix, &n) != 0) {
                        //;VGM_LOG("TXTP:   fade %d^%f~%f=%c@%f~%f+%f~%f\n",
                        //        mix.ch_dst, mix.vol_start, mix.vol_end, mix.shape,
                        //        mix.time_pre, mix.time_start, mix.time_end, mix.time_post);
                        add_mixing(&cfg, &mix, MIX_FADE); /* N^V1~V2@T1~T2+T3~T4: fades volumes between positions */
                        config += n;
                        continue;
                    }

                    break; /* unknown mix/new command/end */
               }
            }
#endif
            else if (strcmp(command,"s") == 0 || (nc == 1 && config[0] >= '0' && config[0] <= '9')) {
                /* subsongs: file.ext#s2 = play subsong 2, file.ext#2~10 = play subsong range */
                int subsong_start = 0, subsong_end = 0;

                //todo also advance config?
                if (sscanf(config, " %d ~ %d", &subsong_start, &subsong_end) == 2) {
                    if (subsong_start > 0 && subsong_end > 0) {
                        range_start = subsong_start-1;
                        range_end = subsong_end;
                    }
                    //;VGM_LOG("TXTP:   subsong range %i~%i\n", range_start, range_end);
                }
                else if (sscanf(config, " %d", &subsong_start) == 1) {
                    if (subsong_start > 0) {
                        range_start = subsong_start-1;
                        range_end = subsong_start;
                    }
                    //;VGM_LOG("TXTP:   subsong single %i-%i\n", range_start, range_end);
                }
                else { /* wrong config, ignore */
                    //;VGM_LOG("TXTP:   subsong none\n");
                }
            }
            else if (strcmp(command,"i") == 0) {
                config += get_bool(config, &cfg.config_ignore_loop);
                //;VGM_LOG("TXTP:   ignore_loop=%i\n", cfg.config_ignore_loop);
            }
            else if (strcmp(command,"E") == 0) {
                config += get_bool(config, &cfg.config_force_loop);
                //;VGM_LOG("TXTP:   force_loop=%i\n", cfg.config_force_loop);
            }
            else if (strcmp(command,"F") == 0) {
                config += get_bool(config, &cfg.config_ignore_fade);
                //;VGM_LOG("TXTP:   ignore_fade=%i\n", cfg.config_ignore_fade);
            }
            else if (strcmp(command,"l") == 0) {
                config += get_double(config, &cfg.config_loop_count);
                //;VGM_LOG("TXTP:   loop_count=%f\n", cfg.config_loop_count);
            }
            else if (strcmp(command,"f") == 0) {
                config += get_double(config, &cfg.config_fade_time);
                //;VGM_LOG("TXTP:   fade_time=%f\n", cfg.config_fade_time);
            }
            else if (strcmp(command,"d") == 0) {
                config += get_double(config, &cfg.config_fade_delay);
                //;VGM_LOG("TXTP:   fade_delay %f\n", cfg.config_fade_delay);
            }
            else if (strcmp(command,"h") == 0) {
                config += get_int(config, &cfg.sample_rate);
                //;VGM_LOG("TXTP:   sample_rate %i\n", cfg.sample_rate);
            }
            else if (strcmp(command,"I") == 0) {
                n = get_time(config,  &cfg.loop_start_second, &cfg.loop_start_sample);
                if (n > 0) { /* first value must exist */
                    config += n;

                    n = get_time(config,  &cfg.loop_end_second, &cfg.loop_end_sample);
                    if (n == 0) { /* second value is optional */
                        cfg.loop_end_max = 1;
                    }

                    config += n;
                    cfg.loop_install = 1;
                }

                //;VGM_LOG("TXTP:   loop_install %i (max=%i): %i %i / %f %f\n", cfg.loop_install, cfg.loop_end_max,
                //        cfg.loop_start_sample, cfg.loop_end_sample, cfg.loop_start_second, cfg.loop_end_second);
            }
#ifdef VGMSTREAM_MIXING
            //todo cleanup
            else if (strcmp(command,"@volume") == 0) {
                txtp_mix_data mix = {0};

                nm = get_double(config, &mix.vol);
                config += nm;

                if (nm == 0) continue;

                nm = get_mask(config, &mix.mask);
                config += nm;

                add_mixing(&cfg, &mix, MACRO_VOLUME);
            }
            else if (strcmp(command,"@track") == 0) {
                txtp_mix_data mix = {0};

                nm = get_mask(config, &mix.mask);
                config += nm;
                if (nm == 0) continue;

                add_mixing(&cfg, &mix, MACRO_TRACK);
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
                add_mixing(&cfg, &mix, MACRO_LAYER);
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

                add_mixing(&cfg, &mix, type);
            }
#endif
            else if (config[nc] == ' ') {
                //;VGM_LOG("TXTP:   comment\n");
                break; /* comment, ignore rest */
            }
            else {
                //;VGM_LOG("TXTP:   unknown command\n");
                break; /* end, incorrect command, or possibly a comment or double ## comment too */
            }
        }
    }


    clean_filename(filename);
    //;VGM_LOG("TXTP: clean filename='%s'\n", filename);

    /* config that applies to all files */
    if (is_default) {
        txtp->default_entry_set = 1;
        add_config(&txtp->default_entry, &cfg, filename);
        return 1;
    }

    /* add filenames */
    for (i = range_start; i < range_end; i++){
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
        if (0==strcmp(val,"layers")) {
            txtp->is_layered = 1;
        }
        else if (0==strcmp(val,"segments")) {
            txtp->is_layered = 0;
        }
        else {
            goto fail;
        }
    }
    else if (0==strcmp(key,"loop_mode")) {
        if (0==strcmp(val,"keep")) {
            txtp->is_loop_keep = 1;
        }
        else {
            goto fail;
        }
    }
    else if (0==strcmp(key,"commands")) {
        char val2[TXTP_LINE_MAX];
        strcpy(val2, val); /* copy since val is modified here but probably not important */
        if (!add_filename(txtp, val2, 1)) goto fail;
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


    /* empty file: use filename with config (ex. "song.ext#3.txtp") */
    if (get_streamfile_size(streamFile) == 0) {
        char filename[PATH_LIMIT] = {0};
        char* ext;
        get_streamfile_filename(streamFile, filename,PATH_LIMIT);

        /* remove ".txtp" */
        ext = strrchr(filename,'.');
        if (!ext) goto fail; /* ??? */
        ext[0] = '\0';

        if (!add_filename(txtp, filename, 0))
            goto fail;

        return txtp;
    }


    /* skip BOM if needed */
    if ((uint16_t)read_16bitLE(0x00, streamFile) == 0xFFFE || (uint16_t)read_16bitLE(0x00, streamFile) == 0xFEFF)
        txt_offset = 0x02;

    /* read lines */
    while (txt_offset < file_size) {
        char line[TXTP_LINE_MAX] = {0};
        char key[TXTP_LINE_MAX] = {0}, val[TXTP_LINE_MAX] = {0}; /* at least as big as a line to avoid overflows (I hope) */
        char filename[TXTP_LINE_MAX] = {0};
        int ok, bytes_read, line_done;

        bytes_read = get_streamfile_text_line(TXTP_LINE_MAX,line, txt_offset,streamFile, &line_done);
        if (!line_done) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trail spaces, # may be commands or comments) */
        ok = sscanf(line, " %[^ \t#=] = %[^\t\r\n] ", key,val);
        if (ok == 2) { /* no key=val */
            if (val[0] != '#') {
                /* val is not command, re-parse skipping comments and trailing spaces */
                ok = sscanf(line, " %[^ \t#=] = %[^ #\t\r\n] ", key,val);
            }
            if (ok == 2) {
                if (!parse_keyval(txtp, key, val)) /* read key/val */
                    goto fail;
                continue;
            }
        }

        /* must be a filename (only remove spaces from start/end, as filenames con contain mid spaces/#/etc) */
        ok = sscanf(line, " %[^\t\r\n] ", filename);
        if (ok != 1) /* not a filename either */
            continue;
        if (filename[0] == '#')
            continue; /* simple comment */

        /* filename with config */
        if (!add_filename(txtp, filename, 0))
            goto fail;
    }


    return txtp;
fail:
    clean_txtp(txtp);
    return NULL;
}

static void clean_txtp(txtp_header* txtp) {
    if (!txtp)
        return;

    free(txtp->entry);
    free(txtp);
}
