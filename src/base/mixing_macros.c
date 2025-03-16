#include <math.h>
#include <limits.h>
#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "../layout/layout.h"
#include "mixing.h"
#include "mixer_priv.h"


#define MIX_MACRO_VOCALS  'v'
#define MIX_MACRO_EQUAL   'e'
#define MIX_MACRO_BGM     'b'

void mixing_macro_volume(VGMSTREAM* vgmstream, double volume, uint32_t mask) {
    mixer_t* mixer = vgmstream->mixer;
    if (!mixer)
        return;

    if (mask == 0) {
        mixing_push_volume(vgmstream, -1, volume);
        return;
    }

    for (int ch = 0; ch < mixer->output_channels; ch++) {
        if (!((mask >> ch) & 1))
            continue;
        mixing_push_volume(vgmstream, ch, volume);
    }
}

void mixing_macro_track(VGMSTREAM* vgmstream, uint32_t mask) {
    mixer_t* mixer = vgmstream->mixer;
    if (!mixer)
        return;

    if (mask == 0) {
        return;
    }

    /* reverse remove all channels (easier this way as when removing channels numbers change) */
    for (int ch = mixer->output_channels - 1; ch >= 0; ch--) {
        if ((mask >> ch) & 1)
            continue;
        mixing_push_downmix(vgmstream, ch);
    }
}


/* get highest channel count */
static int get_layered_max_channels(VGMSTREAM* vgmstream) {
    if (vgmstream->layout_type != layout_layered)
        return 0;

    layered_layout_data* data = vgmstream->layout_data;

    int max = 0;
    for (int i = 0; i < data->layer_count; i++) {
        int output_channels = 0;

        mixing_info(data->layers[i], NULL, &output_channels);

        if (max < output_channels)
            max = output_channels;
    }

    return max;
}

static int is_layered_auto(VGMSTREAM* vgmstream, int max, char mode) {
    if (vgmstream->layout_type != layout_layered)
        return 0;

    /* no channels set and only vocals for now */
    if (max > 0 || mode != MIX_MACRO_VOCALS)
        return 0;

    /* no channel down/upmixing (cannot guess output) */
    mixer_t* mixer = vgmstream->mixer;
    for (int i = 0; i < mixer->chain_count; i++) {
        mix_type_t type = mixer->chain[i].type;
        if (type == MIX_UPMIX || type == MIX_DOWNMIX || type == MIX_KILLMIX) /*type == MIX_SWAP || ??? */
            return 0;
    }

    /* only previsible cases */
    layered_layout_data* l_data = vgmstream->layout_data;
    for (int i = 0; i < l_data->layer_count; i++) {
        int output_channels = 0;

        mixing_info(l_data->layers[i], NULL, &output_channels);

        if (output_channels > 8)
            return 0;
    }

    return 1;
}


/* special layering, where channels are respected (so Ls only go to Ls), also more optimized */
static void mixing_macro_layer_auto(VGMSTREAM* vgmstream, int max, char mode) {
    layered_layout_data* ldata = vgmstream->layout_data;
    int target_layer = 0, target_chs = 0, target_ch = 0, target_silence = 0;
    int ch_num, ch_max;

    /* With N layers like: (ch1 ch2) (ch1 ch2 ch3 ch4) (ch1 ch2), output is normally 2+4+2=8ch.
     * We want to find highest layer (ch1..4) = 4ch, add other channels to it and drop them */

    /* find target "main" channels (will be first most of the time) */
    ch_num = 0;
    ch_max = 0;
    for (int i = 0; i < ldata->layer_count; i++) {
        int layer_chs = 0;

        mixing_info(ldata->layers[i], NULL, &layer_chs);

        if (ch_max < layer_chs || (ch_max == layer_chs && target_silence)) {
            target_ch = ch_num;
            target_chs = layer_chs;
            target_layer = i;
            ch_max = layer_chs;
            /* avoid using silence as main if possible for minor optimization */
            target_silence = (ldata->layers[i]->coding_type == coding_SILENCE);
        }

        ch_num += layer_chs;
    }

    /* all silences? */
    if (!target_chs) {
        target_ch = 0;
        target_chs = 0;
        target_layer = 0;
        mixing_info(ldata->layers[0], NULL, &target_chs);
    }

    /* add other channels to target (assumes standard channel mapping to simplify)
     * most of the time all layers will have same number of channels though */
    ch_num = 0;
    for (int i = 0; i < ldata->layer_count; i++) {
        int layer_chs = 0;

        if (target_layer == i) {
            ch_num += target_chs;
            continue;
        }
        
        mixing_info(ldata->layers[i], NULL, &layer_chs);

        if (ldata->layers[i]->coding_type == coding_SILENCE) {
            ch_num += layer_chs;
            continue; /* unlikely but sometimes in Wwise */
        }

        if (layer_chs == target_chs) {
            /* 1:1 mapping */
            for (int ch = 0; ch < layer_chs; ch++) {
                mixing_push_add(vgmstream, target_ch + ch, ch_num + ch, 1.0);
            }
        }
        else {
            const double vol_sqrt = 1 / sqrt(2);

            /* extra mixing for better sound in some cases (assumes layer_chs is lower than target_chs) */
            switch(layer_chs) {
                case 1:
                    mixing_push_add(vgmstream, target_ch + 0, ch_num + 0, vol_sqrt);
                    mixing_push_add(vgmstream, target_ch + 1, ch_num + 0, vol_sqrt);
                    break;
                case 2:
                    mixing_push_add(vgmstream, target_ch + 0, ch_num + 0, 1.0);
                    mixing_push_add(vgmstream, target_ch + 1, ch_num + 1, 1.0);
                    break;
                default: /* less common */
                    //TODO add other mixes, depends on target_chs + mapping (ex. 4.0 to 5.0 != 5.1, 2.1 xiph to 5.1 != 5.1 xiph)
                    for (int ch = 0; ch < layer_chs; ch++) {
                        mixing_push_add(vgmstream, target_ch + ch, ch_num + ch, 1.0);
                    }
                    break;
            }
        }

        ch_num += layer_chs;
    }

    /* drop non-target channels */
    ch_num = 0;
    for (int i = 0; i < ldata->layer_count; i++) {
        
        if (i < target_layer) { /* least common, hopefully (slower to drop chs 1 by 1) */
            int layer_chs = 0;
            mixing_info(ldata->layers[i], NULL, &layer_chs);

            for (int ch = 0; ch < layer_chs; ch++) {
                mixing_push_downmix(vgmstream, ch_num); //+ ch
            }

            //ch_num += layer_chs; /* dropped channels change this */
        }
        else if (i == target_layer) {
            ch_num += target_chs;
        }
        else { /* most common, hopefully (faster) */
            mixing_push_killmix(vgmstream, ch_num);
            break;
        }
    }
}


void mixing_macro_layer(VGMSTREAM* vgmstream, int max, uint32_t mask, char mode) {
    mixer_t* mixer = vgmstream->mixer;
    int current, ch, output_channels, selected_channels;

    if (!mixer)
        return;

    if (is_layered_auto(vgmstream, max, mode)) {
        //;VGM_LOG("MIX: auto layer mode\n");
        mixing_macro_layer_auto(vgmstream, max, mode);
        return;
    }
    //;VGM_LOG("MIX: regular layer mode\n");

    if (max == 0) /* auto calculate */
        max = get_layered_max_channels(vgmstream);

    if (max <= 0 || mixer->output_channels <= max)
        return;

    /* set all channels (non-existant channels will be ignored) */
    if (mask == 0) {
        mask = ~mask;
    }

    /* save before adding fake channels */
    output_channels = mixer->output_channels;

    /* count possibly set channels */
    selected_channels = 0;
    for (ch = 0; ch < output_channels; ch++) {
        selected_channels += (mask >> ch) & 1;
    }

    /* make N fake channels at the beginning for easier calcs */
    for (ch = 0; ch < max; ch++) {
        mixing_push_upmix(vgmstream, 0);
    }

    /* add all layers in this order: ch0: 0, 0+N, 0+N*2 ... / ch1: 1, 1+N ... */
    current = 0;
    for (ch = 0; ch < output_channels; ch++) {
        double volume = 1.0;

        if (!((mask >> ch) & 1))
            continue;

        /* MIX_MACRO_VOCALS: same volume for all layers (for layered vocals) */
        /* MIX_MACRO_EQUAL: volume adjusted equally for all layers (for generic downmixing) */
        /* MIX_MACRO_BGM: volume adjusted depending on layers (for layered bgm) */
        if (mode == MIX_MACRO_BGM && ch < max) {
            /* reduce a bit main channels (see below) */
            int channel_mixes = selected_channels / max;
            if (current < selected_channels % (channel_mixes * max)) /* may be simplified? */
                channel_mixes += 1;
            channel_mixes -= 1; /* better formula? */
            if (channel_mixes <= 0) /* ??? */
                channel_mixes = 1;

            volume = 1 / sqrt(channel_mixes);
        }
        if ((mode == MIX_MACRO_BGM && ch >= max) || (mode == MIX_MACRO_EQUAL)) {
            /* find how many will be mixed in current channel (earlier channels receive more
             * mixes than later ones, ex: selected 8ch + max 3ch: ch0=0+3+6, ch1=1+4+7, ch2=2+5) */
            int channel_mixes = selected_channels / max;
            if (channel_mixes <= 0) /* ??? */
                channel_mixes = 1;
            if (current < selected_channels % (channel_mixes * max)) /* may be simplified? */
                channel_mixes += 1;

            volume = 1 / sqrt(channel_mixes); /* "power" add */
        }
        //;VGM_LOG("MIX: layer ch=%i, cur=%i, v=%f\n", ch, current, volume);

        mixing_push_add(vgmstream, current, max + ch, volume); /* ch adjusted considering upmixed channels */
        current++;
        if (current >= max)
            current = 0;
    }

    /* remove all mixed channels */
    mixing_push_killmix(vgmstream, max);
}

void mixing_macro_crosstrack(VGMSTREAM* vgmstream, int max) {
    mixer_t* mixer = vgmstream->mixer;
    int current, ch, track, track_ch, track_num, output_channels;
    int32_t change_pos, change_next, change_time;

    if (!mixer)
        return;
    if (max <= 0 || mixer->output_channels <= max)
        return;
    if (!vgmstream->loop_flag) /* maybe force loop? */
        return;

    /* this probably only makes sense for even channels so upmix before if needed) */
    output_channels = mixer->output_channels;
    if (output_channels % 2) {
        mixing_push_upmix(vgmstream, output_channels);
        output_channels += 1;
    }

    /* set loops to hear all track changes */
    track_num = output_channels / max;
    if (vgmstream->config.loop_count < track_num) {
        vgmstream->config.loop_count = track_num;
        vgmstream->config.loop_count_set = 1;
        vgmstream->config.config_set = 1;
    }

    ch = 0;
    for (track = 0; track < track_num; track++) {
        double volume = 1.0; /* won't play at the same time, no volume change needed */

        int loop_pre = vgmstream->loop_start_sample;
        int loop_samples = vgmstream->loop_end_sample - vgmstream->loop_start_sample;
        change_pos = loop_pre + loop_samples * track;
        change_next = loop_pre + loop_samples * (track + 1);
        change_time = 15.0 * vgmstream->sample_rate; /* in secs */

        for (track_ch = 0; track_ch < max; track_ch++) {
            if (track > 0) { /* fade-in when prev track fades-out */
                mixing_push_fade(vgmstream, ch + track_ch, 0.0, volume, '(', -1, change_pos, change_pos + change_time, -1);
            }

            if (track + 1 < track_num) { /* fade-out when next track fades-in */
                mixing_push_fade(vgmstream, ch + track_ch, volume, 0.0, ')', -1, change_next, change_next + change_time, -1);
            }
        }

        ch += max;
    }

    /* mix all tracks into first */
    current = 0;
    for (ch = max; ch < output_channels; ch++) {
        mixing_push_add(vgmstream, current, ch, 1.0); /* won't play at the same time, no volume change needed */

        current++;
        if (current >= max)
            current = 0;
    }

    /* remove unneeded channels */
    mixing_push_killmix(vgmstream, max);
}

void mixing_macro_crosslayer(VGMSTREAM* vgmstream, int max, char mode) {
    mixer_t* mixer = vgmstream->mixer;
    int current, ch, layer, layer_ch, layer_num, loop, output_channels;
    int32_t change_pos, change_time;

    if (!mixer)
        return;
    if (max <= 0 || mixer->output_channels <= max)
        return;
    if (!vgmstream->loop_flag) /* maybe force loop? */
        return;

    /* this probably only makes sense for even channels so upmix before if needed) */
    output_channels = mixer->output_channels;
    if (output_channels % 2) {
        mixing_push_upmix(vgmstream, output_channels);
        output_channels += 1;
    }

    /* set loops to hear all track changes */
    layer_num = output_channels / max;
    if (vgmstream->config.loop_count < layer_num) {
        vgmstream->config.loop_count = layer_num;
        vgmstream->config.loop_count_set = 1;
        vgmstream->config.config_set = 1;
    }

    /* MIX_MACRO_VOCALS: constant volume
     * MIX_MACRO_EQUAL: sets fades to successively lower/equalize volume per loop for each layer
     * (to keep final volume constant-ish), ex. 3 layers/loops, 2 max:
     * - layer0 (ch0+1): loop0 --[1.0]--, loop1 )=1.0..0.7, loop2 )=0.7..0.5, loop3 --[0.5/end]--
     * - layer1 (ch2+3): loop0 --[0.0]--, loop1 (=0.0..0.7, loop2 )=0.7..0.5, loop3 --[0.5/end]--
     * - layer2 (ch4+5): loop0 --[0.0]--, loop1 ---[0.0]--, loop2 (=0.0..0.5, loop3 --[0.5/end]--
     * MIX_MACRO_BGM: similar but 1st layer (main) has higher/delayed volume:
     * - layer0 (ch0+1): loop0 --[1.0]--, loop1 )=1.0..1.0, loop2 )=1.0..0.7, loop3 --[0.7/end]--
     */
    for (loop = 1; loop < layer_num; loop++) {
        double volume1 = 1.0;
        double volume2 = 1.0;

        int loop_pre = vgmstream->loop_start_sample;
        int loop_samples = vgmstream->loop_end_sample - vgmstream->loop_start_sample;
        change_pos = loop_pre + loop_samples * loop;
        change_time = 10.0 * vgmstream->sample_rate; /* in secs */

        if (mode == MIX_MACRO_EQUAL) {
            volume1 = 1 / sqrt(loop + 0);
            volume2 = 1 / sqrt(loop + 1);
        }

        ch = 0;
        for (layer = 0; layer < layer_num; layer++) {
            char type;

            if (mode == MIX_MACRO_BGM) {
                if (layer == 0) {
                    volume1 = 1 / sqrt(loop - 1 <= 0 ? 1 : loop - 1);
                    volume2 = 1 / sqrt(loop + 0);
                }
                else {
                    volume1 = 1 / sqrt(loop + 0);
                    volume2 = 1 / sqrt(loop + 1);
                }
            }

            if (layer > loop) { /* not playing yet (volume is implicitly 0.0 from first fade in) */
                continue;
            } else if (layer == loop) { /* fades in for the first time */
                volume1 = 0.0;
                type = '(';
            } else { /* otherwise fades out to match other layers's volume */
                type = ')';
            }

            //;VGM_LOG("MIX: loop=%i, layer %i, vol1=%f, vol2=%f\n", loop, layer, volume1, volume2);

            for (layer_ch = 0; layer_ch < max; layer_ch++) {
                mixing_push_fade(vgmstream, ch + layer_ch, volume1, volume2, type, -1, change_pos, change_pos + change_time, -1);
            }

            ch += max;
        }
    }

    /* mix all tracks into first */
    current = 0;
    for (ch = max; ch < output_channels; ch++) {
        mixing_push_add(vgmstream, current, ch, 1.0);

        current++;
        if (current >= max)
            current = 0;
    }

    /* remove unneeded channels */
    mixing_push_killmix(vgmstream, max);
}


typedef enum {
    pos_FL  = 0,
    pos_FR  = 1,
    pos_FC  = 2,
    pos_LFE = 3,
    pos_BL  = 4,
    pos_BR  = 5,
    pos_FLC = 6,
    pos_FRC = 7,
    pos_BC  = 8,
    pos_SL  = 9,
    pos_SR  = 10,
} mixing_position_t;

void mixing_macro_downmix(VGMSTREAM* vgmstream, int max /*, mapping_t output_mapping*/) {
    mixer_t* mixer = vgmstream->mixer;
    int output_channels, mp_in, mp_out, ch_in, ch_out;
    channel_mapping_t input_mapping, output_mapping;
    const double vol_max = 1.0;
    const double vol_sqrt = 1 / sqrt(2);
    const double vol_half = 0.5;
    double matrix[16][16] = {{0}};


    if (!mixer)
        return;
    if (max <= 1 || mixer->output_channels <= max || max >= 8)
        return;

    /* assume WAV defaults if not set */
    input_mapping = vgmstream->channel_layout;
    if (input_mapping == 0) {
        switch(mixer->output_channels) {
            case 1: input_mapping = mapping_MONO; break;
            case 2: input_mapping = mapping_STEREO; break;
            case 3: input_mapping = mapping_2POINT1; break;
            case 4: input_mapping = mapping_QUAD; break;
            case 5: input_mapping = mapping_5POINT0; break;
            case 6: input_mapping = mapping_5POINT1; break;
            case 7: input_mapping = mapping_7POINT0; break;
            case 8: input_mapping = mapping_7POINT1; break;
            default: return;
        }
    }

    /* build mapping matrix[input channel][output channel] = volume,
     * using standard WAV/AC3 downmix formulas
     * - https://www.audiokinetic.com/library/edge/?source=Help&id=downmix_tables
     * - https://www.audiokinetic.com/library/edge/?source=Help&id=standard_configurations
     */
    switch(max) {
        case 1:
            output_mapping = mapping_MONO;
            matrix[pos_FL][pos_FC] = vol_sqrt;
            matrix[pos_FR][pos_FC] = vol_sqrt;
            matrix[pos_FC][pos_FC] = vol_max;
            matrix[pos_SL][pos_FC] = vol_half;
            matrix[pos_SR][pos_FC] = vol_half;
            matrix[pos_BL][pos_FC] = vol_half;
            matrix[pos_BR][pos_FC] = vol_half;
            break;
        case 2:
            output_mapping = mapping_STEREO;
            matrix[pos_FL][pos_FL] = vol_max;
            matrix[pos_FR][pos_FR] = vol_max;
            matrix[pos_FC][pos_FL] = vol_sqrt;
            matrix[pos_FC][pos_FR] = vol_sqrt;
            matrix[pos_SL][pos_FL] = vol_sqrt;
            matrix[pos_SR][pos_FR] = vol_sqrt;
            matrix[pos_BL][pos_FL] = vol_sqrt;
            matrix[pos_BR][pos_FR] = vol_sqrt;
            break;
        default:
            /* not sure if +3ch would use FC/LFE, SL/BR and whatnot without passing extra config, so ignore for now */
            return;
    }

    /* save and make N fake channels at the beginning for easier calcs */
    output_channels = mixer->output_channels;
    for (int ch = 0; ch < max; ch++) {
        mixing_push_upmix(vgmstream, 0);
    }

    /* downmix */
    ch_in = 0;
    for (mp_in = 0; mp_in < 16; mp_in++) {
        /* read input mapping (ex. 5.1) and find channel */
        if (!(input_mapping & (1 << mp_in)))
            continue;

        ch_out = 0;
        for (mp_out = 0; mp_out < 16; mp_out++) {
            /* read output mapping (ex. 2.0) and find channel */
            if (!(output_mapping & (1 << mp_out)))
                continue;
            mixing_push_add(vgmstream, ch_out, max + ch_in, matrix[mp_in][mp_out]);

            ch_out++;
            if (ch_out > max)
                break;
        }

        ch_in++;
        if (ch_in >= output_channels)
            break;
    }

    /* remove unneeded channels */
    mixing_push_killmix(vgmstream, max);
}


void mixing_macro_output_sample_format(VGMSTREAM* vgmstream, sfmt_t type) {
    mixer_t* mixer = vgmstream->mixer;
    if (!mixer || !type)
        return;

    // optimization (may skip initializing mixer)
    sfmt_t input_fmt = mixing_get_input_sample_type(vgmstream);
    if (input_fmt == type)
        return;
    mixer->force_type = type;
    mixer->has_non_fade = true;
}
