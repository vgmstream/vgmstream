#include <math.h>

#include "txtp.h"
#include "../util/text_reader.h"
#include "../util/paths.h"

#define TXT_LINE_MAX 2048 /* some wwise .txtp get wordy */
#define TXT_LINE_KEY_MAX 128
#define TXT_LINE_VAL_MAX (TXT_LINE_MAX - TXT_LINE_KEY_MAX)


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

static int get_double(const char* params, double* value, bool* is_set) {
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

static int get_time_f(const char* params, double* value_f, int32_t* value_i, bool* flag) {
    int n = get_time(params, value_f, value_i);
    if (n > 0)
        *flag = 1;
    return n;
}

static int get_bool(const char* params, bool* value) {
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


static int get_fade(const char* params, txtp_mix_data_t* mix, int* p_n) {
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

    // needs final length
    mix->time_end = mix->time_start + mix->time_end;
    mix->sample_end = mix->sample_start + mix->sample_end;

    *p_n = tn;
    return 1;
fail:
    return 0;
}

/*******************************************************************************/
/* PARSER - MAIN                                                               */
/*******************************************************************************/

static void add_settings(txtp_entry_t* current, txtp_entry_t* entry, const char* filename) {

    /* don't memcopy to allow list additions and ignore values not set, as current can be "default" settings */
    //*current = *cfg;

    if (filename)
        strcpy(current->filename, filename);


    /* play config */
    txtp_copy_config(&current->config, &entry->config);

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

    current->body_mode = entry->body_mode;
}

//TODO use
static inline int is_match(const char* str1, const char* str2) {
    return strcmp(str1, str2) == 0;
}

static void parse_params(txtp_entry_t* entry, char* params) {
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
                txtp_mix_data_t mix = {0};

                //;VGM_LOG("TXTP: subcommand='%s'\n", params);

                //todo use strchr instead?
                if (sscanf(params, " %c%n", &cmd, &n) == 1 && n != 0 && cmd == ',') {
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d - %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i-%i\n", mix.ch_dst, mix.ch_src);
                    txtp_add_mixing(entry, &mix, MIX_SWAP); /* N-M: swaps M with N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d + %d * %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0) ||
                    (sscanf(params, " %d + %d x %lf%n", &mix.ch_dst, &mix.ch_src, &mix.vol, &n) == 3 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i+%i*%f\n", mix.ch_dst, mix.ch_src, mix.vol);
                    txtp_add_mixing(entry, &mix, MIX_ADD_VOLUME); /* N+M*V: mixes M*volume to N */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d + %d%n", &mix.ch_dst, &mix.ch_src, &n) == 2 && n != 0) {
                    //;VGM_LOG("TXTP:   mix %i+%i\n", mix.ch_dst, mix.ch_src);
                    txtp_add_mixing(entry, &mix, MIX_ADD); /* N+M: mixes M to N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d * %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0) ||
                    (sscanf(params, " %d x %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i*%f\n", mix.ch_dst, mix.vol);
                    txtp_add_mixing(entry, &mix, MIX_VOLUME); /* N*V: changes volume of N */
                    params += n;
                    continue;
                }

                if ((sscanf(params, " %d = %lf%n", &mix.ch_dst, &mix.vol, &n) == 2 && n != 0)) {
                    //;VGM_LOG("TXTP:   mix %i=%f\n", mix.ch_dst, mix.vol);
                    txtp_add_mixing(entry, &mix, MIX_LIMIT); /* N=V: limits volume of N */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'D') {
                    //;VGM_LOG("TXTP:   mix %iD\n", mix.ch_dst);
                    txtp_add_mixing(entry, &mix, MIX_KILLMIX); /* ND: downmix N and all following channels */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'd') {
                    //;VGM_LOG("TXTP:   mix %id\n", mix.ch_dst);
                    txtp_add_mixing(entry, &mix, MIX_DOWNMIX);/* Nd: downmix N only */
                    params += n;
                    continue;
                }

                if (sscanf(params, " %d%c%n", &mix.ch_dst, &cmd, &n) == 2 && n != 0 && cmd == 'u') {
                    //;VGM_LOG("TXTP:   mix %iu\n", mix.ch_dst);
                    txtp_add_mixing(entry, &mix, MIX_UPMIX); /* Nu: upmix N */
                    params += n;
                    continue;
                }

                if (get_fade(params, &mix, &n) != 0) {
                    //;VGM_LOG("TXTP:   fade %d^%f~%f=%c@%f~%f+%f~%f\n",
                    //        mix.ch_dst, mix.vol_start, mix.vol_end, mix.shape,
                    //        mix.time_pre, mix.time_start, mix.time_end, mix.time_post);
                    txtp_add_mixing(entry, &mix, MIX_FADE); /* N^V1~V2@T1~T2+T3~T4: fades volumes between positions */
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
            txtp_mix_data_t mix = {0};

            nm = get_volume(params, &mix.vol, NULL);
            params += nm;

            if (nm == 0) continue;

            nm = get_mask(params, &mix.mask);
            params += nm;

            txtp_add_mixing(entry, &mix, MACRO_VOLUME);
        }
        else if (strcmp(command,"@track") == 0 ||
                 strcmp(command,"C") == 0 ) {
            txtp_mix_data_t mix = {0};

            nm = get_mask(params, &mix.mask);
            params += nm;
            if (nm == 0) continue;

            txtp_add_mixing(entry, &mix, MACRO_TRACK);
        }
        else if (strcmp(command,"@layer-v") == 0 ||
                 strcmp(command,"@layer-b") == 0 ||
                 strcmp(command,"@layer-e") == 0) {
            txtp_mix_data_t mix = {0};

            nm = get_int(params, &mix.max);
            params += nm;

            if (nm > 0) { /* max is optional (auto-detects and uses max channels) */
                nm = get_mask(params, &mix.mask);
                params += nm;
            }

            mix.mode = command[7]; /* pass letter */
            txtp_add_mixing(entry, &mix, MACRO_LAYER);
        }
        else if (strcmp(command,"@crosslayer-v") == 0 ||
                 strcmp(command,"@crosslayer-b") == 0 ||
                 strcmp(command,"@crosslayer-e") == 0 ||
                 strcmp(command,"@crosstrack") == 0) {
            txtp_mix_data_t mix = {0};
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

            txtp_add_mixing(entry, &mix, type);
        }
        else if (strcmp(command,"@downmix") == 0) {
            txtp_mix_data_t mix = {0};

            mix.max = 2; /* stereo only for now */
            //nm = get_int(params, &mix.max);
            //params += nm;
            //if (nm == 0) continue;

            txtp_add_mixing(entry, &mix, MACRO_DOWNMIX);
        }
        else if (strcmp(command,"@body-intro") == 0) {
            entry->body_mode = TXTP_BODY_INTRO;
            VGM_LOG("body: %x\n", entry->body_mode);
        }
        else if (strcmp(command,"@body-main") == 0) {
            entry->body_mode = TXTP_BODY_MAIN;
        }
        else if (strcmp(command,"@body-outro") == 0) {
            entry->body_mode = TXTP_BODY_OUTRO;
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


static int add_group(txtp_header_t* txtp, char* line) {
    int n, m;
    txtp_group_t cfg = {0};
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
            txtp_group_t *temp_group;

            txtp->group_max += 5;
            temp_group = realloc(txtp->group, sizeof(txtp_group_t) * txtp->group_max);
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
static int add_entry(txtp_header_t* txtp, char* filename, int is_default) {
    int i;
    txtp_entry_t entry = {0};


    //;VGM_LOG("TXTP: input filename=%s\n", filename);

    /* parse filename: file.ext#(commands) */
    {
        char* params;

        if (is_default) {
            params = filename; /* multiple commands without filename */
        }
        else {
            // Find settings after filename (basically find extension then first #).
            // Filenames may contain dots and # though, so this may be fooled by certain patterns
            // (like with extensionless files with a # inside or dirs with . in the name)

            // Find first dot which is usually the extension; may be a false positive but hard to handle every case
            // (can't use "last dot" because some commands allow it like '#I 1.0 20.0')
            params = strchr(filename, '.');
            if (!params) // extensionless = reset to line start
                params = filename;

            // Skip relative path like ./../stuff/../ and maybe "01 blah... blah.adx"
            while (params[1] == '.' || params[1] == '/') {
                char* params_tmp = strchr(params + 1, '.');
                if (!params_tmp) //???
                    break;
                params = params_tmp;
            }

            // Rarely filenames may be "01. blah (#blah).ext #i", where the first # is ambiguous.
            // Detect the space after dot (always a track number) and search dot again.
            if (params[1] == ' ') {
                params = strchr(params + 1, '.');
                if (!params) /* extensionless */
                    params = filename;
            }

            // first # after dot should be actual .txtp settings
            params = strchr(params, '#');
            if (!params)
                params = NULL;
        }

        //;VGM_LOG("TXTP: params=%s\n", params);
        parse_params(&entry, params);
    }

    //;VGM_LOG("TXTP: output filename=%s\n", filename);

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
        txtp_entry_t* current;

        /* resize in steps if not enough */
        if (txtp->entry_count+1 > txtp->entry_max) {
            txtp_entry_t* temp_entry;

            txtp->entry_max += 5;
            temp_entry = realloc(txtp->entry, sizeof(txtp_entry_t) * txtp->entry_max);
            if (!temp_entry) goto fail;
            txtp->entry = temp_entry;
        }

        /* new entry */
        current = &txtp->entry[txtp->entry_count];
        memset(current,0, sizeof(txtp_entry_t));
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

static int parse_keyval(txtp_header_t* txtp, const char* key, const char* val) {
    //;VGM_LOG("TXTP: key=val '%s'='%s'\n", key,val);


    if (0==strcmp(key,"loop_start_segment")) {
        if (!parse_num(val, &txtp->loop_start_segment)) goto fail;
    }
    else if (0==strcmp(key,"loop_end_segment")) {
        if (!parse_num(val, &txtp->loop_end_segment)) goto fail;
    }
    else if (0==strcmp(key,"mode")) {
        if (is_substring(val,"layers")) {
            txtp->is_segmented = false;
            txtp->is_layered = true;
        }
        else if (is_substring(val,"segments")) {
            txtp->is_segmented = true;
            txtp->is_layered = false;
        }
        else if (is_substring(val,"mixed")) {
            txtp->is_segmented = false;
            txtp->is_layered = false;
        }
        else {
            goto fail;
        }
    }
    else if (0==strcmp(key,"loop_mode")) {
        if (is_substring(val,"keep")) {
            txtp->is_loop_keep = true;
        }
        else if (is_substring(val,"auto")) {
            txtp->is_loop_auto = true;
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
        // in rare cases a filename may contain a (blah=blah.blah), but it's hard to distinguish
        // from key=val + setting with dots. Signal unknown command to treat it like a file (should fail later).
        return -1;
    }

    return 1;
fail:
    VGM_LOG("TXTP: error while parsing key=val '%s'='%s'\n", key,val);
    return 0;
}

txtp_header_t* txtp_parse(STREAMFILE* sf) {
    txtp_header_t* txtp = NULL;
    uint32_t txt_offset;


    txtp = calloc(1,sizeof(txtp_header_t));
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
                int ret = parse_keyval(txtp, key, val); /* read key/val */
                if (ret == 0) goto fail;
                if (ret > 0)
                    continue;
                // ret < 0: try to handle as filename below
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
    txtp_clean(txtp);
    return NULL;
}
