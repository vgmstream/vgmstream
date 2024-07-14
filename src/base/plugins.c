#include "../vgmstream.h"
#include "../util/log.h"
#include "../util/reader_sf.h"
#include "../util/reader_text.h"
#include "plugins.h"
#include "mixing.h"


/* ****************************************** */
/* CONTEXT: simplifies plugin code            */
/* ****************************************** */

int vgmstream_ctx_is_valid(const char* filename, vgmstream_ctx_valid_cfg *cfg) {
    const char** extension_list;
    size_t extension_list_len;
    const char* extension;
    int i;

    bool is_extension = cfg && cfg->is_extension;
    bool reject_extensionless = cfg && cfg->reject_extensionless;
    bool skip_standard = cfg && cfg->skip_standard;
    bool accept_common = cfg && cfg->accept_common;
    bool accept_unknown = cfg && cfg->accept_common;

    if (is_extension) {
        extension = filename;
    } else {
        extension = filename_extension(filename);
    }

    /* some metas accept extensionless files, but make sure it's not a path (unlikely but...) */
    if (strlen(extension) <= 0) {
        int len = strlen(filename); /* foobar passes an extension as so len may be still 0 */
        if (len <= 0 && is_extension)
            return 0;
        if (len > 1 && (filename[len - 1] == '/' || filename[len - 1] == '\\'))
            return 0;
        return !reject_extensionless;
    }

    /* try in default list */
    if (!skip_standard) {
        extension_list = vgmstream_get_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                return 1;
            }
        }
    }

    /* try in common extensions */
    if (accept_common) {
        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0)
                return 1;
        }
    }

    /* allow anything not in the normal list but not in common extensions */
    if (accept_unknown) {
        int is_common = 0;

        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                is_common = 1;
                break;
            }
        }

        if (!is_common)
            return 1;
    }

    return 0;
}

void vgmstream_get_title(char* buf, int buf_len, const char* filename, VGMSTREAM* vgmstream, vgmstream_title_t* cfg) {
    const char* pos;
    char* pos2;
    char temp[1024];

    if (!buf || !buf_len)
        return;

    buf[0] = '\0';
    if (!vgmstream || !filename)
        return;

    /* name without path */
    pos = strrchr(filename, '\\');
    if (!pos)
        pos = strrchr(filename, '/');
    if (!pos)
        pos = filename;
    else
        pos++;

    /* special case for foobar that uses a (archive)|(subfile) notation when opening a 7z/zip/etc directly */
    if (cfg && cfg->remove_archive) {
        const char* subpos = strchr(pos, '|');
        if (subpos)
            pos = subpos + 1;
    }
    strncpy(buf, pos, buf_len);

    /* name without extension */
    if (cfg && cfg->remove_extension) {
        pos2 = strrchr(buf, '.');
        if (pos2 && strlen(pos2) < 15) /* too big extension = file name probably has a dot in the middle */
            pos2[0] = '\0';
    }

    {
        const char* stream_name = vgmstream->stream_name;
        int total_subsongs = vgmstream->num_streams;
        int target_subsong = vgmstream->stream_index;
        //int is_first = vgmstream->stream_index == 0;
        int show_name;

        /* special considerations for TXTP:
         * - full txtp: don't show subsong number, nor name (assumes one names .txtp as wanted)
         * - mini txtp: don't show subsong number, but show name (assumes one choses song #n in filename, but wants title)
         */
        int full_txtp = vgmstream->config.is_txtp && !vgmstream->config.is_mini_txtp;
        int mini_txtp = vgmstream->config.is_mini_txtp;

        if (target_subsong == 0)
            target_subsong = 1;

        /* show number if file has more than 1 subsong */
        if (total_subsongs > 1 && !(full_txtp || mini_txtp)) {
            if (cfg && cfg->subsong_range)
                snprintf(temp, sizeof(temp), "%s#1~%i", buf, total_subsongs);
            else
                snprintf(temp, sizeof(temp), "%s#%i", buf, target_subsong);
            strncpy(buf, temp, buf_len);
        }

        /* show name for some cases */
        show_name = (total_subsongs > 0) && (!cfg || !cfg->subsong_range);
        if (full_txtp)
            show_name = 0;
        if (cfg && cfg->force_title)
            show_name = 1;

        if (stream_name[0] != '\0' && show_name) {
            snprintf(temp, sizeof(temp), "%s (%s)", buf, stream_name);
            strncpy(buf, temp, buf_len);
        }
    }

    buf[buf_len - 1] = '\0';
}


static void copy_time(int* dst_flag, int32_t* dst_time, double* dst_time_s, int* src_flag, int32_t* src_time, double* src_time_s) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_time = *src_time;
    *dst_time_s = *src_time_s;
}

//todo reuse in txtp?
static void load_default_config(play_config_t* def, play_config_t* tcfg) {

    /* loop limit: txtp #L > txtp #l > player #L > player #l */
    if (tcfg->play_forever) {
        def->play_forever = 1;
        def->ignore_loop = 0;
    }
    if (tcfg->loop_count_set) {
        def->loop_count = tcfg->loop_count;
        def->loop_count_set = 1;
        def->ignore_loop = 0;
        if (!tcfg->play_forever)
            def->play_forever = 0;
    }

    /* fade priority: #F > #f, #d */
    if (tcfg->ignore_fade) {
        def->ignore_fade = 1;
    }
    if (tcfg->fade_delay_set) {
        def->fade_delay = tcfg->fade_delay;
        def->fade_delay_set = 1;
    }
    if (tcfg->fade_time_set) {
        def->fade_time = tcfg->fade_time;
        def->fade_time_set = 1;
    }

    /* loop priority: #i > #e > #E (respect player's ignore too) */
    if (tcfg->really_force_loop) {
        //def->ignore_loop = 0;
        def->force_loop = 0;
        def->really_force_loop = 1;
    }
    if (tcfg->force_loop) {
        //def->ignore_loop = 0;
        def->force_loop = 1;
        def->really_force_loop = 0;
    }
    if (tcfg->ignore_loop) {
        def->ignore_loop = 1;
        def->force_loop = 0;
        def->really_force_loop = 0;
    }

    copy_time(&def->pad_begin_set,  &def->pad_begin,    &def->pad_begin_s,      &tcfg->pad_begin_set,   &tcfg->pad_begin,   &tcfg->pad_begin_s);
    copy_time(&def->pad_end_set,    &def->pad_end,      &def->pad_end_s,        &tcfg->pad_end_set,     &tcfg->pad_end,     &tcfg->pad_end_s);
    copy_time(&def->trim_begin_set, &def->trim_begin,   &def->trim_begin_s,     &tcfg->trim_begin_set,  &tcfg->trim_begin,  &tcfg->trim_begin_s);
    copy_time(&def->trim_end_set,   &def->trim_end,     &def->trim_end_s,       &tcfg->trim_end_set,    &tcfg->trim_end,    &tcfg->trim_end_s);
    copy_time(&def->body_time_set,  &def->body_time,    &def->body_time_s,      &tcfg->body_time_set,   &tcfg->body_time,   &tcfg->body_time_s);

    def->is_mini_txtp = tcfg->is_mini_txtp;
    def->is_txtp = tcfg->is_txtp;
}

static void load_player_config(play_config_t* def, vgmstream_cfg_t* vcfg) {
    def->play_forever = vcfg->play_forever;
    def->ignore_loop = vcfg->ignore_loop;
    def->force_loop = vcfg->force_loop;
    def->really_force_loop = vcfg->really_force_loop;
    def->ignore_fade = vcfg->ignore_fade;

    def->loop_count = vcfg->loop_count;
    def->loop_count_set = 1;
    def->fade_delay = vcfg->fade_delay;
    def->fade_delay_set = 1;
    def->fade_time = vcfg->fade_time;
    def->fade_time_set = 1;
}

void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* vcfg) {
    play_config_t defs = {0};
    play_config_t* def = &defs; /* for convenience... */
    play_config_t* tcfg = &vgmstream->config;


    load_player_config(def, vcfg);
    def->config_set = 1;

    if (!vcfg->disable_config_override)
        load_default_config(def, tcfg);

    if (!vcfg->allow_play_forever)
        def->play_forever = 0;

    /* copy final config back */
     *tcfg = *def;

     vgmstream->config_enabled = def->config_set;
     setup_state_vgmstream(vgmstream);
}

/* ****************************************** */
/* MIXING: modifies vgmstream output          */
/* ****************************************** */

void vgmstream_mixing_enable(VGMSTREAM* vgmstream, int32_t max_sample_count, int *input_channels, int *output_channels) {
    mixing_setup(vgmstream, max_sample_count);
    mixing_info(vgmstream, input_channels, output_channels);

    /* update internals */
    mixing_info(vgmstream, &vgmstream->pstate.input_channels, &vgmstream->pstate.output_channels);
    setup_vgmstream(vgmstream);
}

void vgmstream_mixing_autodownmix(VGMSTREAM *vgmstream, int max_channels) {
    if (max_channels <= 0)
        return;

    /* guess mixing the best we can, using standard downmixing if possible
     * (without mapping we can't be sure if format is using a standard layout) */
    if (vgmstream->channel_layout && max_channels <= 2) {
        mixing_macro_downmix(vgmstream, max_channels);
    }
    else {
        mixing_macro_layer(vgmstream, max_channels, 0, 'e');
    }

    return;
}

void vgmstream_mixing_stereo_only(VGMSTREAM *vgmstream, int start) {
    if (start < 0)
        return;
    /* could check to avoid making mono files in edge cases but meh */

    /* remove channels before start */
    while (start) {
        mixing_push_downmix(vgmstream, 0);
        start--;
    }
    /* remove channels after stereo */
    mixing_push_killmix(vgmstream, start + 2);
}


/* ****************************************** */
/* LOG: log                                   */
/* ****************************************** */

void vgmstream_set_log_callback(int level, void* callback) {
    vgm_log_set_callback(NULL, level, 0, callback);
}

void vgmstream_set_log_stdout(int level) {
    vgm_log_set_callback(NULL, level, 1, NULL);
}
