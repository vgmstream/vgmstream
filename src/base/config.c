#include "../vgmstream.h"
#include "../util/log.h"
#include "plugins.h"
#include "mixing.h"



static void copy_time(bool* dst_flag, int32_t* dst_time, double* dst_time_s, bool* src_flag, int32_t* src_time, double* src_time_s) {
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
