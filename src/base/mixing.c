#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixing_priv.h"
#include "mixing_fades.h"
#include "plugins.h"
#include <math.h>
#include <limits.h>


/**
 * Mixing lets vgmstream modify the resulting sample buffer before final output.
 * This can be implemented in a number of ways but it's done like it is considering
 * overall simplicity in coding, usage and performance (main complexity is allowing
 * down/upmixing). Code is mostly independent with some hooks in the main vgmstream
 * code.
 *
 * It works using two buffers:
 * - outbuf: plugin's pcm16 buffer, at least input_channels*sample_count
 * - mixbuf: internal's pcmfloat buffer, at least mixing_channels*sample_count
 * outbuf starts with decoded samples of vgmstream->channel size. This unsures that
 * if no mixing is done (most common case) we can skip copying samples between buffers.
 * Resulting outbuf after mixing has samples for ->output_channels (plus garbage).
 * - output_channels is the resulting total channels (that may be less/more/equal)
 * - input_channels is normally ->channels or ->output_channels when it's higher
 *
 * First, a meta (ex. TXTP) or plugin may add mixing commands through the API,
 * validated so non-sensical mixes are ignored (to ensure mixing code doesn't
 * have to recheck every time). Then, before starting to decode mixing must be
 * manually activated, because plugins need to be ready for possibly different
 * input/output channels. API could be improved but this way we can avoid having
 * to update all plugins, while allowing internal setup and layer/segment mixing
 * (may change in the future for simpler usage).
 *
 * Then after decoding normally, vgmstream applies mixing internally:
 * - detect if mixing is active and needs to be done at this point (some effects
 *   like fades only apply after certain time) and skip otherwise.
 * - copy outbuf to mixbuf, as using a float buffer to increase accuracy (most ops
 *   apply float volumes) and slightly improve performance (avoids doing
 *   int16-to-float casts per mix, as it's not free)
 * - apply all mixes on mixbuf
 * - copy mixbuf to outbuf
 * segmented/layered layouts handle mixing on their own.
 *
 * Mixing is tuned for most common case (no mix except fade-out at the end) and is
 * fast enough but not super-optimized yet, there is some penalty the more effects
 * are applied. Maybe could add extra sub-ops to avoid ifs and dumb values (volume=0.0
 * could simply use a clear op), only use mixbuf if necessary (swap can be done without
 * mixbuf if it goes first) or add function pointer indexes but isn't too important.
 * Operations are applied once per "step" with 1 sample from all channels to simplify code
 * (and maybe improve memory cache?), though maybe it should call one function per operation.
 */

/* ******************************************************************* */

static void sbuf_copy_f32_to_s16(int16_t* buf_s16, float* buf_f32, int samples, int channels) {
    for (int s = 0; s < samples * channels; s++) {
        /* when casting float to int, value is simply truncated:
         * - (int)1.7 = 1, (int)-1.7 = -1
         * alts for more accurate rounding could be:
         * - (int)floor(f)
         * - (int)(f < 0 ? f - 0.5f : f + 0.5f)
         * - (((int) (f1 + 32768.5)) - 32768)
         * - etc
         * but since +-1 isn't really audible we'll just cast as it's the fastest
         */
        buf_s16[s] = clamp16( (int32_t)buf_f32[s] );
    }
}

void mix_vgmstream(sample_t *outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {
    mixing_data *data = vgmstream->mixing_data;
    int ch, s, m, ok;

    int32_t current_subpos = 0;
    float temp_f, temp_min, temp_max, cur_vol = 0.0f;
    float *temp_mixbuf;
    sample_t *temp_outbuf;

    const float limiter_max = 32767.0f;
    const float limiter_min = -32768.0f;

    /* no support or not need to apply */
    if (!data || !data->mixing_on || data->mixing_count == 0)
        return;

    /* try to skip if no fades apply (set but does nothing yet) + only has fades */
    if (data->has_fade) {
        int32_t current_pos = get_current_pos(vgmstream, sample_count);
        //;VGM_LOG("MIX: fade test %i, %i\n", data->has_non_fade, is_fade_active(data, current_pos, current_pos + sample_count));
        if (!data->has_non_fade && !is_fade_active(data, current_pos, current_pos + sample_count))
            return;
        //;VGM_LOG("MIX: fade pos=%i\n", current_pos);
        current_subpos = current_pos;
    }


    /* use advancing buffer pointers to simplify logic */
    temp_mixbuf = data->mixbuf; /* you'd think using a int32 temp buf would be faster but somehow it's slower? */
    temp_outbuf = outbuf;

    /* mixing ops are designed to apply in order, all channels per 1 sample 'step'. Since some ops change
     * total channels, channel number meaning varies as ops move them around, ex:
     * - 4ch w/ "1-2,2+3" = ch1<>ch3, ch2(old ch1)+ch3 = 4ch: ch2 ch1+ch3 ch3 ch4
     * - 4ch w/ "2+3,1-2" = ch2+ch3, ch1<>ch2(modified) = 4ch: ch2+ch3 ch1 ch3 ch4
     * - 2ch w/ "1+2,1u" = ch1+ch2, ch1(add and push rest) = 3ch: ch1' ch1+ch2 ch2
     * - 2ch w/ "1u,1+2" = ch1(add and push rest) = 3ch: ch1'+ch1 ch1 ch2
     * - 2ch w/ "1-2,1d" = ch1<>ch2, ch1(drop and move ch2(old ch1) to ch1) = ch1
     * - 2ch w/ "1d,1-2" = ch1(drop and pull rest), ch1(do nothing, ch2 doesn't exist now) = ch2
     */

    /* apply mixes in order per channel */
    for (s = 0; s < sample_count; s++) {
        /* reset after new sample 'step'*/
        float *stpbuf = temp_mixbuf;
        int step_channels = vgmstream->channels;

        for (ch = 0; ch < step_channels; ch++) {
            stpbuf[ch] = temp_outbuf[ch]; /* copy current 'lane' */
        }

        for (m = 0; m < data->mixing_count; m++) {
            mix_command_data *mix = &data->mixing_chain[m];

            switch(mix->command) {

                case MIX_SWAP:
                    temp_f = stpbuf[mix->ch_dst];
                    stpbuf[mix->ch_dst] = stpbuf[mix->ch_src];
                    stpbuf[mix->ch_src] = temp_f;
                    break;

                case MIX_ADD:
                    stpbuf[mix->ch_dst] = stpbuf[mix->ch_dst] + stpbuf[mix->ch_src] * mix->vol;
                    break;

                case MIX_ADD_COPY:
                    stpbuf[mix->ch_dst] = stpbuf[mix->ch_dst] + stpbuf[mix->ch_src];
                    break;

                case MIX_VOLUME:
                    if (mix->ch_dst < 0) {
                        for (ch = 0; ch < step_channels; ch++) {
                            stpbuf[ch] = stpbuf[ch] * mix->vol;
                        }
                    }
                    else {
                        stpbuf[mix->ch_dst] = stpbuf[mix->ch_dst] * mix->vol;
                    }
                    break;

                case MIX_LIMIT:
                    temp_max = limiter_max * mix->vol;
                    temp_min = limiter_min * mix->vol;

                    if (mix->ch_dst < 0) {
                        for (ch = 0; ch < step_channels; ch++) {
                            if (stpbuf[ch] > temp_max)
                                stpbuf[ch] = temp_max;
                            else if (stpbuf[ch] < temp_min)
                                stpbuf[ch] = temp_min;
                        }
                    }
                    else {
                        if (stpbuf[mix->ch_dst] > temp_max)
                            stpbuf[mix->ch_dst] = temp_max;
                        else if (stpbuf[mix->ch_dst] < temp_min)
                            stpbuf[mix->ch_dst] = temp_min;
                    }
                    break;

                case MIX_UPMIX:
                    step_channels += 1;
                    for (ch = step_channels - 1; ch > mix->ch_dst; ch--) {
                        stpbuf[ch] = stpbuf[ch-1]; /* 'push' channels forward (or pull backwards) */
                    }
                    stpbuf[mix->ch_dst] = 0; /* inserted as silent */
                    break;

                case MIX_DOWNMIX:
                    step_channels -= 1;
                    for (ch = mix->ch_dst; ch < step_channels; ch++) {
                        stpbuf[ch] = stpbuf[ch+1]; /* 'pull' channels back */
                    }
                    break;

                case MIX_KILLMIX:
                    step_channels = mix->ch_dst; /* clamp channels */
                    break;

                case MIX_FADE:
                    ok = get_fade_gain(mix, &cur_vol, current_subpos);
                    if (!ok) {
                        break; /* fade doesn't apply right now */
                    }

                    if (mix->ch_dst < 0) {
                        for (ch = 0; ch < step_channels; ch++) {
                            stpbuf[ch] = stpbuf[ch] * cur_vol;
                        }
                    }
                    else {
                        stpbuf[mix->ch_dst] = stpbuf[mix->ch_dst] * cur_vol;
                    }
                    break;

                default:
                    break;
            }
        }

        current_subpos++;

        temp_mixbuf += step_channels;
        temp_outbuf += vgmstream->channels;
    }

    /* copy resulting temp mix to output */
    sbuf_copy_f32_to_s16(outbuf, data->mixbuf, sample_count, data->output_channels);
}

/* ******************************************************************* */

void mixing_init(VGMSTREAM* vgmstream) {
    mixing_data *data = calloc(1, sizeof(mixing_data));
    if (!data) goto fail;

    data->mixing_size = VGMSTREAM_MAX_MIXING; /* fixed array for now */
    data->mixing_channels = vgmstream->channels;
    data->output_channels = vgmstream->channels;

    vgmstream->mixing_data = data;
    return;

fail:
    free(data);
    return;
}

void mixing_close(VGMSTREAM* vgmstream) {
    mixing_data *data = NULL;
    if (!vgmstream) return;

    data = vgmstream->mixing_data;
    if (!data) return;

    free(data->mixbuf);
    free(data);
}

void mixing_update_channel(VGMSTREAM* vgmstream) {
    mixing_data *data = vgmstream->mixing_data;
    if (!data) return;

    /* lame hack for dual stereo, but dual stereo is pretty hack-ish to begin with */
    data->mixing_channels++;
    data->output_channels++;
}


/* ******************************************************************* */

static int fix_layered_channel_layout(VGMSTREAM* vgmstream) {
    int i;
    mixing_data* data = vgmstream->mixing_data;
    layered_layout_data* layout_data;
    uint32_t prev_cl;

    if (vgmstream->channel_layout || vgmstream->layout_type != layout_layered)
        return 0;
  
    layout_data = vgmstream->layout_data;

    /* mainly layer-v (in cases of layers-within-layers should cascade) */
    if (data->output_channels != layout_data->layers[0]->channels)
        return 0;

    /* check all layers share layout (implicitly works as a channel check, if not 0) */
    prev_cl = layout_data->layers[0]->channel_layout;
    if (prev_cl == 0)
        return 0;

    for (i = 1; i < layout_data->layer_count; i++) {
        uint32_t layer_cl = layout_data->layers[i]->channel_layout;
        if (prev_cl != layer_cl)
            return 0;
        
        prev_cl = layer_cl;
    }

    vgmstream->channel_layout = prev_cl;
    return 1;
}

/* channel layout + down/upmixing = ?, salvage what we can */
static void fix_channel_layout(VGMSTREAM* vgmstream) {
    mixing_data* data = vgmstream->mixing_data;

    if (fix_layered_channel_layout(vgmstream))
        goto done;

    /* segments should share channel layout automatically */

    /* a bit wonky but eh... */
    if (vgmstream->channel_layout && vgmstream->channels != data->output_channels) {
        vgmstream->channel_layout = 0;
    }

done:
    ((VGMSTREAM*)vgmstream->start_vgmstream)->channel_layout = vgmstream->channel_layout;
}

void mixing_setup(VGMSTREAM* vgmstream, int32_t max_sample_count) {
    mixing_data *data = vgmstream->mixing_data;
    float *mixbuf_re = NULL;

    if (!data) goto fail;

    /* special value to not actually enable anything (used to query values) */
    if (max_sample_count <= 0)
        goto fail;

    /* create or alter internal buffer */
    mixbuf_re = realloc(data->mixbuf, max_sample_count*data->mixing_channels*sizeof(float));
    if (!mixbuf_re) goto fail;

    data->mixbuf = mixbuf_re;
    data->mixing_on = 1;

    fix_channel_layout(vgmstream);

    /* since data exists on its own memory and pointer is already set
     * there is no need to propagate to start_vgmstream */

    /* segments/layers are independant from external buffers and may always mix */

    return;
fail:
    return;
}

void mixing_info(VGMSTREAM* vgmstream, int* p_input_channels, int* p_output_channels) {
    mixing_data *data = vgmstream->mixing_data;
    int input_channels, output_channels;

    if (!data) goto fail;

    output_channels = data->output_channels;
    if (data->output_channels > vgmstream->channels)
        input_channels = data->output_channels;
    else
        input_channels = vgmstream->channels;

    if (p_input_channels)  *p_input_channels = input_channels;
    if (p_output_channels) *p_output_channels = output_channels;

    //;VGM_LOG("MIX: channels %i, in=%i, out=%i, mix=%i\n", vgmstream->channels, input_channels, output_channels, data->mixing_channels);
    return;
fail:
    if (p_input_channels)  *p_input_channels = vgmstream->channels;
    if (p_output_channels) *p_output_channels = vgmstream->channels;
    return;
}
