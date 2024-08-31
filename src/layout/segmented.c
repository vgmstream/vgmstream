#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/mixing.h"
#include "../base/plugins.h"
#include "../base/sbuf.h"
#include "../base/render.h"

#define VGMSTREAM_MAX_SEGMENTS 1024
#define VGMSTREAM_SEGMENT_SAMPLE_BUFFER 8192


/* Decodes samples for segmented streams.
 * Chains together sequential vgmstreams, for data divided into separate sections or files
 * (like one part for intro and other for loop segments, which may even use different codecs). */
void render_vgmstream_segmented(sbuf_t* sbuf, VGMSTREAM* vgmstream) {
    segmented_layout_data* data = vgmstream->layout_data;
    sbuf_t ssrc_tmp;
    sbuf_t* ssrc = &ssrc_tmp;


    if (data->current_segment >= data->segment_count) {
        VGM_LOG_ONCE("SEGMENT: wrong current segment\n");
        sbuf_silence_rest(sbuf);
        return;
    }

    int current_channels = 0;
    mixing_info(data->segments[data->current_segment], NULL, &current_channels);
    int samples_this_block = vgmstream_get_samples(data->segments[data->current_segment]);

    while (sbuf->filled < sbuf->samples) {
        int samples_to_do;
        sfmt_t segment_format;
        void* buf_filled = NULL;

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping (loop_layout has been called below, changes segments/state) */
            samples_this_block = vgmstream_get_samples(data->segments[data->current_segment]);
            mixing_info(data->segments[data->current_segment], NULL, &current_channels);
            continue;
        }

        /* detect segment change and restart (after loop, but before decode, to allow looping to kick in) */
        if (vgmstream->samples_into_block >= samples_this_block) {
            data->current_segment++;

            if (data->current_segment >= data->segment_count) { /* when decoding more than num_samples */
                VGM_LOG_ONCE("SEGMENTED: reached last segment\n");
                goto decode_fail;
            }

            /* in case of looping spanning multiple segments */
            reset_vgmstream(data->segments[data->current_segment]);

            samples_this_block = vgmstream_get_samples(data->segments[data->current_segment]);
            mixing_info(data->segments[data->current_segment], NULL, &current_channels);
            vgmstream->samples_into_block = 0;
            continue;
        }


        samples_to_do = decode_get_samples_to_do(samples_this_block, sbuf->samples, vgmstream);
        if (samples_to_do > sbuf->samples - sbuf->filled)
            samples_to_do = sbuf->samples - sbuf->filled;
        if (samples_to_do > VGMSTREAM_SEGMENT_SAMPLE_BUFFER /*&& use_internal_buffer*/) /* always for fade/etc mixes */
            samples_to_do = VGMSTREAM_SEGMENT_SAMPLE_BUFFER;

        if (samples_to_do < 0) { /* 0 is ok? */
            VGM_LOG_ONCE("SEGMENTED: wrong samples_to_do %i found\n", samples_to_do);
            goto decode_fail;
        }

        segment_format = mixing_get_input_sample_type(data->segments[data->current_segment]);
        sbuf_init(ssrc, segment_format, data->buffer, samples_to_do, data->segments[data->current_segment]->channels);

        // try to use part of outbuf directly if not remixed (minioptimization) //TODO improve detection
        if (vgmstream->channels == data->input_channels && sbuf->fmt == segment_format && !data->mixed_channels) {
            buf_filled = sbuf_get_filled_buf(sbuf);
            ssrc->buf = buf_filled;
        }

        render_main(ssrc, data->segments[data->current_segment]);

        // returned buf may have changed
        if (ssrc->buf != buf_filled) {
            sbuf_copy_segments(sbuf, ssrc);
        }

        sbuf->filled += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }

    return;
decode_fail:
    sbuf_silence_rest(sbuf);
}


void seek_layout_segmented(VGMSTREAM* vgmstream, int32_t seek_sample) {
    segmented_layout_data* data = vgmstream->layout_data;

    int segment = 0;
    int total_samples = 0;
    while (total_samples < vgmstream->num_samples) {
        int32_t segment_samples = vgmstream_get_samples(data->segments[segment]);

        /* find if sample falls within segment's samples */
        if (seek_sample >= total_samples && seek_sample < total_samples + segment_samples) {
            int32_t seek_relative = seek_sample - total_samples;

            seek_vgmstream(data->segments[segment], seek_relative);
            data->current_segment = segment;
            vgmstream->samples_into_block = seek_relative;
            break;
        }

        total_samples += segment_samples;
        segment++;
    }

    // ???
    VGM_ASSERT(segment == data->segment_count, "SEGMENTED: can't find seek segment\n");
}

void loop_layout_segmented(VGMSTREAM* vgmstream, int32_t loop_sample) {
    seek_layout_segmented(vgmstream, loop_sample);
}


segmented_layout_data* init_layout_segmented(int segment_count) {
    segmented_layout_data* data = NULL;

    if (segment_count <= 0 || segment_count > VGMSTREAM_MAX_SEGMENTS)
        goto fail;

    data = calloc(1, sizeof(segmented_layout_data));
    if (!data) goto fail;

    data->segments = calloc(segment_count, sizeof(VGMSTREAM*));
    if (!data->segments) goto fail;

    data->segment_count = segment_count;
    data->current_segment = 0;

    return data;
fail:
    free_layout_segmented(data);
    return NULL;
}

bool setup_layout_segmented(segmented_layout_data* data) {
    int max_input_channels = 0;
    int max_output_channels = 0;
    int max_sample_size = 0;
    bool mixed_channels = false;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (int i = 0; i < data->segment_count; i++) {

        if (data->segments[i] == NULL) {
            VGM_LOG("SEGMENTED: no vgmstream in segment %i\n", i);
            return false;
        }

        if (data->segments[i]->num_samples <= 0) {
            VGM_LOG("SEGMENTED: no samples in segment %i\n", i);
            return false;
        }

        /* allow config if set for fine-tuned parts (usually TXTP only) */
        data->segments[i]->config_enabled = data->segments[i]->config.config_set;

        /* disable so that looping is controlled by render_vgmstream_segmented */
        if (data->segments[i]->loop_flag != 0) {
            VGM_LOG("SEGMENTED: segment %i is looped\n", i);

            /* config allows internal loops */
            if (!data->segments[i]->config_enabled) {
                data->segments[i]->loop_flag = 0;
            }
        }

        /* different segments may have different input or output channels (in rare cases of using ex. 2ch + 4ch) */
        int segment_input_channels, segment_output_channels;
        mixing_info(data->segments[i], &segment_input_channels, &segment_output_channels);
        if (max_input_channels < segment_input_channels)
            max_input_channels = segment_input_channels;
        if (max_output_channels < segment_output_channels)
            max_output_channels = segment_output_channels;

        if (i > 0) {
            int prev_output_channels;

            mixing_info(data->segments[i-1], NULL, &prev_output_channels);
            if (segment_output_channels != prev_output_channels) {
                mixed_channels = true;
                //VGM_LOG("SEGMENTED: segment %i has wrong channels %i vs prev channels %i\n", i, segment_output_channels, prev_output_channels);
                //goto fail;
            }

            /* a bit weird, but no matter (should resample) */
            if (data->segments[i]->sample_rate != data->segments[i-1]->sample_rate) {
                VGM_LOG("SEGMENTED: segment %i has different sample rate\n", i);
            }

            /* perfectly acceptable */
            //if (data->segments[i]->coding_type != data->segments[i-1]->coding_type)
            //    goto fail;
        }

        int current_sample_size = sfmt_get_sample_size( mixing_get_input_sample_type(data->segments[i]) );
        if (max_sample_size < current_sample_size)
            max_sample_size = current_sample_size;

        /* init mixing */
        mixing_setup(data->segments[i], VGMSTREAM_SEGMENT_SAMPLE_BUFFER);

        /* final setup in case the VGMSTREAM was created manually */
        setup_vgmstream(data->segments[i]);
    }

    if (max_output_channels > VGMSTREAM_MAX_CHANNELS || max_input_channels > VGMSTREAM_MAX_CHANNELS)
        return false;

    /* create internal buffer big enough for mixing */
    free(data->buffer);
    data->buffer = malloc(VGMSTREAM_SEGMENT_SAMPLE_BUFFER * max_input_channels * max_sample_size);
    if (!data->buffer) goto fail;

    data->input_channels = max_input_channels;
    data->output_channels = max_output_channels;
    data->mixed_channels = mixed_channels;

    return true;
fail:
    return false; /* caller is expected to free */
}

void free_layout_segmented(segmented_layout_data* data) {
    if (!data)
        return;

    for (int i = 0; i < data->segment_count; i++) {
        bool is_repeat = false;

        /* segments are allowed to be repeated so don't close the same thing twice */
        for (int j = 0; j < i; j++) {
            if (data->segments[i] == data->segments[j]) {
                is_repeat = true;
                break;
            }
        }

        if (is_repeat)
            continue;
        close_vgmstream(data->segments[i]);
    }
    free(data->segments);
    free(data->buffer);
    free(data);
}

void reset_layout_segmented(segmented_layout_data* data) {
    if (!data)
        return;

    for (int i = 0; i < data->segment_count; i++) {
        reset_vgmstream(data->segments[i]);
    }

    data->current_segment = 0;
}
