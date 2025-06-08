#include <math.h>

#include "txtp.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../base/mixing.h"
#include "../base/plugins.h"
#include "../util/layout_utils.h"


/*******************************************************************************/
/* CONFIG                                                                      */
/*******************************************************************************/


static void apply_settings_body(VGMSTREAM* vgmstream, txtp_entry_t* entry) {
    // tweak playable part, which only makes sense 

    if (!entry->body_mode || !vgmstream->loop_flag)
        return;

    entry->config.fade_time_set = false;
    entry->config.fade_delay_set = false;
    entry->config.ignore_fade = false;
    entry->config.ignore_loop = true;

    switch(entry->body_mode) {
        case TXTP_BODY_INTRO:
            if (vgmstream->loop_start_sample == 0)
                return;
            entry->trim_set = true;
            entry->trim_sample = vgmstream->loop_start_sample;

            entry->config.config_set = true;
            break;

        case TXTP_BODY_MAIN:
            entry->config.trim_begin_set = true;
            entry->config.trim_begin = vgmstream->loop_start_sample;
            entry->trim_set = true;
            entry->trim_sample = vgmstream->loop_end_sample;

            entry->config.config_set = true;
            break;

        case TXTP_BODY_OUTRO:
            if (vgmstream->loop_end_sample >= vgmstream->num_samples)
                return;
            entry->config.trim_begin_set = true;
            entry->config.trim_begin = vgmstream->loop_end_sample;

            entry->config.config_set = true;
            break;
        default:
            break;
    }
}

static void apply_settings(VGMSTREAM* vgmstream, txtp_entry_t* current) {

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

    apply_settings_body(vgmstream, current);

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
        for (int ch = 0; ch < vgmstream->channels; ch++) {
            if (!((current->channel_mask >> ch) & 1)) {
                txtp_mix_data_t mix = {0};
                mix.ch_dst = ch + 1;
                mix.vol = 0.0f;
                txtp_add_mixing(current, &mix, MIX_VOLUME);
            }
        }
    }

    /* apply play config (after sample rate/etc mods) */
    txtp_copy_config(&vgmstream->config, &current->config);
    setup_vgmstream_play_state(vgmstream);
    // config is enabled in layouts or externally (for compatibility, since we don't know yet if this
    // VGMSTREAM will part of a layout, or is enabled externally to not mess up plugins's calcs)

    /* apply mixing (last as some mixes depend on config like loops/etc, shouldn't matter much) */
    for (int m = 0; m < current->mixing_count; m++) {
        txtp_mix_data_t* mix = &current->mixing[m];

        switch(mix->command) {
            // base mixes
            case MIX_SWAP:       mixing_push_swap(vgmstream, mix->ch_dst, mix->ch_src); break;
            case MIX_ADD:        mixing_push_add(vgmstream, mix->ch_dst, mix->ch_src, 1.0); break;
            case MIX_ADD_VOLUME: mixing_push_add(vgmstream, mix->ch_dst, mix->ch_src, mix->vol); break;
            case MIX_VOLUME:     mixing_push_volume(vgmstream, mix->ch_dst, mix->vol); break;
            case MIX_LIMIT:      mixing_push_limit(vgmstream, mix->ch_dst, mix->vol); break;
            case MIX_UPMIX:      mixing_push_upmix(vgmstream, mix->ch_dst); break;
            case MIX_DOWNMIX:    mixing_push_downmix(vgmstream, mix->ch_dst); break;
            case MIX_KILLMIX:    mixing_push_killmix(vgmstream, mix->ch_dst); break;
            case MIX_FADE:
                // Convert from time to samples now that sample rate is final.
                // Samples and time values may be mixed though, so it's done for every
                // value (if one is 0 the other will be too, though)
                if (mix->time_pre > 0.0)   mix->sample_pre = mix->time_pre * vgmstream->sample_rate;
                if (mix->time_start > 0.0) mix->sample_start = mix->time_start * vgmstream->sample_rate;
                if (mix->time_end > 0.0)   mix->sample_end = mix->time_end * vgmstream->sample_rate;
                if (mix->time_post > 0.0)  mix->sample_post = mix->time_post * vgmstream->sample_rate;
                // convert special meaning too
                if (mix->time_pre < 0.0)   mix->sample_pre = -1;
                if (mix->time_post < 0.0)  mix->sample_post = -1;

                if (mix->position_type == TXTP_POSITION_LOOPS && vgmstream->loop_flag) {
                    int loop_pre = vgmstream->loop_start_sample;
                    int loop_samples = (vgmstream->loop_end_sample - vgmstream->loop_start_sample);

                    int position_samples = loop_pre + loop_samples * mix->position;

                    if (mix->sample_pre >= 0) mix->sample_pre += position_samples;
                    mix->sample_start += position_samples;
                    mix->sample_end += position_samples;
                    if (mix->sample_post >= 0) mix->sample_post += position_samples;
                }

                mixing_push_fade(vgmstream,
                    mix->ch_dst, mix->vol_start, mix->vol_end, mix->shape,
                    mix->sample_pre, mix->sample_start, mix->sample_end, mix->sample_post);
                break;

            // macro mixes
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

    /* save final config */
    setup_vgmstream(vgmstream);
}


/*******************************************************************************/
/* ENTRIES                                                                     */
/*******************************************************************************/

static bool parse_silents(txtp_header_t* txtp) {
    VGMSTREAM* v_base = NULL;

    /* silents use same channels as close files */
    for (int i = 0; i < txtp->vgmstream_count; i++) {
        if (!txtp->entry[i].silent) {
            v_base = txtp->vgmstream[i];
            break;
        }
    }

    /* actually open silents */
    for (int i = 0; i < txtp->vgmstream_count; i++) {
        if (!txtp->entry[i].silent)
            continue;

        txtp->vgmstream[i] = init_vgmstream_silence_base(v_base);
        if (!txtp->vgmstream[i]) goto fail;

        apply_settings(txtp->vgmstream[i], &txtp->entry[i]);
    }

    return true;
fail:
    return false;
}

static bool is_silent(const char* fn) {
    /* should also contain "." in the filename for commands with seconds ("1.0") to work */
    return fn[0] == '?';
}

static bool is_absolute(const char* fn) {
    return fn[0] == '/' || fn[0] == '\\'  || fn[1] == ':';
}

/* open all entries and apply settings to resulting VGMSTREAMs */
static bool parse_entries(txtp_header_t* txtp, STREAMFILE* sf) {
    bool has_silents = false;


    if (txtp->entry_count == 0)
        goto fail;

    txtp->vgmstream = calloc(txtp->entry_count, sizeof(VGMSTREAM*));
    if (!txtp->vgmstream) goto fail;

    txtp->vgmstream_count = txtp->entry_count;


    /* open all entry files first as they'll be modified by modes */
    for (int i = 0; i < txtp->vgmstream_count; i++) {
        STREAMFILE* temp_sf = NULL;
        const char* filename = txtp->entry[i].filename;

        /* silent entry ignore */
        if (is_silent(filename)) {
            txtp->entry[i].silent = true;
            has_silents = true;
            continue;
        }

        /* absolute paths are detected for convenience, but since it's hard to unify all OSs
         * and plugins, they aren't "officially" supported nor documented, thus may or may not work */
        if (is_absolute(filename))
            temp_sf = open_streamfile(sf, filename); /* from path as is */
        else
            temp_sf = open_streamfile_by_pathname(sf, filename); /* from current path */
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

    return true;
fail:
    return false;
}


/*******************************************************************************/
/* GROUPS                                                                      */
/*******************************************************************************/

static void update_vgmstream_list(VGMSTREAM* vgmstream, txtp_header_t* txtp, int position, int count) {
    //;VGM_LOG("TXTP: compact position=%i count=%i, vgmstreams=%i\n", position, count, txtp->vgmstream_count);

    /* sets and compacts vgmstream list pulling back all following entries */
    txtp->vgmstream[position] = vgmstream;
    for (int i = position + count; i < txtp->vgmstream_count; i++) {
        //;VGM_LOG("TXTP: copy %i to %i\n", i, i + 1 - count);
        txtp->vgmstream[i + 1 - count] = txtp->vgmstream[i];
        txtp->entry[i + 1 - count] = txtp->entry[i]; /* memcpy old settings for other groups */
    }

    /* list can only become smaller, no need to alloc/free/etc */
    txtp->vgmstream_count = txtp->vgmstream_count + 1 - count;
    //;VGM_LOG("TXTP: compact vgmstreams=%i\n", txtp->vgmstream_count);
}

static bool find_loop_anchors(txtp_header_t* txtp, int position, int count, int* p_loop_start, int* p_loop_end) {
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
        return true;
    }

    return false;
}


static bool make_group_segment(txtp_header_t* txtp, txtp_group_t* grp, int position, int count) {
    VGMSTREAM* vgmstream = NULL;
    segmented_layout_data* data_s = NULL;
    int loop_flag = 0;
    int loop_start = 0, loop_end = 0;


    /* allowed for actual groups (not final "mode"), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return true;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored segment position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return true;
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


    /* fix loop keep (do it before init'ing as loops/metadata may be disabled for segments) */
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    if (loop_flag && txtp->is_loop_keep) {
        int32_t current_samples = 0;
        for (int i = 0; i < count; i++) {
            if (loop_start == i+1 /*&& txtp->vgmstream[i + position]->loop_start_sample*/) {
                loop_start_sample = current_samples + txtp->vgmstream[i + position]->loop_start_sample;
            }

            current_samples += txtp->vgmstream[i + position]->num_samples;

            if (loop_end == i+1 && txtp->vgmstream[i + position]->loop_end_sample) {
                loop_end_sample = current_samples - txtp->vgmstream[i + position]->num_samples + txtp->vgmstream[i + position]->loop_end_sample;
            }
        }
    }

    /* init layout */
    data_s = init_layout_segmented(count);
    if (!data_s) goto fail;

    /* copy each subfile */
    for (int i = 0; i < count; i++) {
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
    for (int i = 0; i < count; i++) {
        if (vgmstream->meta_type != data_s->segments[i]->meta_type) {
            vgmstream->meta_type = meta_TXTP;
            break;
        }
    }

    /* fix loop keep */
    if (loop_flag && txtp->is_loop_keep) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;
    }


    /* set new vgmstream and reorder positions */
    update_vgmstream_list(vgmstream, txtp, position, count);


    /* special "whole loop" settings */
    if (grp && grp->entry.loop_anchor_start == 1) {
        grp->entry.config.config_set = 1;
        grp->entry.config.really_force_loop = 1;
    }

    return true;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_segmented(data_s);
    return false;
}

static bool make_group_layer(txtp_header_t* txtp, txtp_group_t* grp, int position, int count) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data_l = NULL;


    /* allowed for actual groups (not final mode), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return true;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored layer position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return 1;
    }


    /* init layout */
    data_l = init_layout_layered(count);
    if (!data_l) goto fail;

    /* copy each subfile */
    for (int i = 0; i < count; i++) {
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
    for (int i = 0; i < count; i++) {
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

    return true;
fail:
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data_l);
    return false;
}

static int make_group_random(txtp_header_t* txtp, txtp_group_t* grp, int position, int count, int selected) {
    VGMSTREAM* vgmstream = NULL;

    /* allowed for actual groups (not final mode), otherwise skip to optimize */
    if (!grp && count == 1) {
        //;VGM_LOG("TXTP: ignored single group\n");
        return true;
    }

    if (position + count > txtp->vgmstream_count || position < 0 || count < 0) {
        VGM_LOG("TXTP: ignored random position=%i, count=%i, entries=%i\n", position, count, txtp->vgmstream_count);
        return true;
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
        for (int i = 0; i < count; i++) {
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

    return true;
fail:
    close_vgmstream(vgmstream);
    return false;
}

static bool parse_groups(txtp_header_t* txtp) {

    /* detect single files before grouping */
    if (txtp->group_count == 0 && txtp->vgmstream_count == 1) {
        txtp->is_single = true;
        txtp->is_segmented = false;
        txtp->is_layered = false;
    }

    /* group files as needed */
    for (int i = 0; i < txtp->group_count; i++) {
        txtp_group_t *grp = &txtp->group[i];
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

    return true;
fail:
    return false;
}


bool txtp_process(txtp_header_t* txtp, STREAMFILE* sf) {
    bool ok;

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

    return true;
fail:
    return false;
}
