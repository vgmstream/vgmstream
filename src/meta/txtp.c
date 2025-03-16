#include "meta.h"
#include "txtp.h"


/* TXTP - an artificial playlist-like format to play files with segments/layers/config */
VGMSTREAM* init_vgmstream_txtp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    txtp_header_t* txtp = NULL;
    bool ok;

    /* checks */
    if (!check_extensions(sf, "txtp"))
        goto fail;

    /* read .txtp with all files and settings */
    txtp = txtp_parse(sf);
    if (!txtp) goto fail;

    /* apply settings to get final vgmstream */
    ok = txtp_process(txtp, sf);
    if (!ok) goto fail;


    /* should result in a final, single vgmstream possibly containing multiple vgmstreams */
    vgmstream = txtp->vgmstream[0];
    txtp->vgmstream[0] = NULL;

    /* flags for title config */
    vgmstream->config.is_txtp = true;
    vgmstream->config.is_mini_txtp = (get_streamfile_size(sf) == 0);

    txtp_clean(txtp);
    return vgmstream;
fail:
    txtp_clean(txtp);
    return NULL;
}


void txtp_clean(txtp_header_t* txtp) {
    if (!txtp)
        return;

    /* first vgmstream may be NULL on success */
    for (int i = 0; i < txtp->vgmstream_count; i++) {
        close_vgmstream(txtp->vgmstream[i]);
    }

    free(txtp->vgmstream);
    free(txtp->group);
    free(txtp->entry);
    free(txtp);
}


static void copy_flag(bool* dst_flag, bool* src_flag) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
}

static void copy_secs(bool* dst_flag, double* dst_secs, bool* src_flag, double* src_secs) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_secs = *src_secs;
}

static void copy_time(bool* dst_flag, int32_t* dst_time, double* dst_time_s, bool* src_flag, int32_t* src_time, double* src_time_s) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_time = *src_time;
    *dst_time_s = *src_time_s;
}

void txtp_copy_config(play_config_t* dst, play_config_t* src) {
    if (!src->config_set)
        return;

    // "no loops" (intro only) and "ignore fade" can't work due to how decoding works, must use @body-* macros
    if (src->loop_count_set && src->loop_count == 0 && src->ignore_fade) {
        src->ignore_fade = false;
    }

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

void txtp_add_mixing(txtp_entry_t* entry, txtp_mix_data_t* mix, txtp_mix_t command) {
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
