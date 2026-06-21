#include "layout.h"
#include "../vgmstream.h"
#include "../base/seek.h"
#include "../base/decode.h"
#include "../base/mixing.h"
#include "../base/play_state.h"
#include "../base/render.h"

#define VGMSTREAM_MAX_SEGMENTS 1024
#define VGMSTREAM_SEGMENT_SAMPLE_BUFFER 8192


/* Decodes samples for segmented streams.
 * Chains together sequential vgmstreams, for data divided into separate sections or files
 * (like one part for intro and other for loop segments, which may even use different codecs). */
rc_t render_layout_segmented(sbuf_t* sbuf, VGMSTREAM* vgmstream) {
    segmented_layout_data* data = vgmstream->layout_data;
    sbuf_t ssrc_tmp;
    sbuf_t* ssrc = &ssrc_tmp;


    if (data->current_segment >= data->segment_count) {
        VGM_LOG_ONCE("SEGMENT: wrong current segment\n");
        return RC_LAYOUT_ERROR;
    }

    int current_channels = 0;
    VGMSTREAM* vs = data->segments[data->current_segment];
    mixing_info(vs, NULL, &current_channels);
    int samples_this_block = vgmstream_get_samples(vs);

    while (sbuf->filled < sbuf->samples) {

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle loop end to start */
            loop_layout_segmented(vgmstream, vgmstream->loop_current_sample);

            // update temp vars, since state changed in decode_do_loop > loop_layout_segmented
            vs = data->segments[data->current_segment];
            samples_this_block = vgmstream_get_samples(vs);
            mixing_info(vs, NULL, &current_channels);

            //;VGM_LOG("SEGMENTED: loop point\n");
            continue;
        }

        /* detect segment change and restart (after loop, but before decode, to allow looping to kick in) */
        if (vgmstream->samples_into_block >= samples_this_block) {
            //;VGM_LOG("SEGMENTED: next segment\n");
            data->current_segment++;

            if (data->current_segment >= data->segment_count) { /* when decoding more than num_samples */
                VGM_LOG_ONCE("SEGMENTED: reached last segment, into=%i, this=%i, curr=%i\n", vgmstream->samples_into_block, samples_this_block, data->current_segment);
                return RC_LAYOUT_ERROR;
            }

            vs = data->segments[data->current_segment];
            reset_vgmstream(vs); // in case of looping spanning multiple segments

            samples_this_block = vgmstream_get_samples(vs);
            mixing_info(vs, NULL, &current_channels);
            vgmstream->samples_into_block = 0;
            continue;
        }

        // needed for decode_do_loop
        int samples_to_do = decode_get_samples_to_do(samples_this_block, sbuf->samples, vgmstream);
        if (samples_to_do > sbuf->samples - sbuf->filled)
            samples_to_do = sbuf->samples - sbuf->filled;
        if (samples_to_do > VGMSTREAM_SEGMENT_SAMPLE_BUFFER /*&& use_internal_buffer*/) /* always for fade/etc mixes */
            samples_to_do = VGMSTREAM_SEGMENT_SAMPLE_BUFFER;

        if (samples_to_do < 0) { /* 0 is ok? */
            VGM_LOG_ONCE("SEGMENTED: wrong samples_to_do %i found\n", samples_to_do);
            return RC_LAYOUT_ERROR;
        }

        vs = data->segments[data->current_segment];

        sfmt_t segment_format = mixing_get_input_sample_type(vs);
        sbuf_init(ssrc, segment_format, data->buffer, samples_to_do, vs->channels);

        // try to use part of outbuf directly if not remixed (minioptimization) //TODO improve detection
        void* buf_filled = NULL;
        if (vgmstream->channels == data->input_channels && sbuf->fmt == segment_format && !data->mixed_channels) {
            buf_filled = sbuf_get_filled_buf(sbuf);
            ssrc->buf = buf_filled;
        }

        //TODO: buf may be smaller than samples
        rc_t rc = render_main(ssrc, vs);

        // returned buf may have changed
        if (ssrc->buf != buf_filled) {
            sbuf_copy_segments(sbuf, ssrc, ssrc->filled);
        } else {
            //TODO ???
            sbuf->filled += ssrc->filled;
        }

        vgmstream->current_sample += ssrc->filled;
        vgmstream->samples_into_block += ssrc->filled;
    }

    return RC_RENDER_OK;
}


/* seeks inside the streams */
//TODO: looping doesn't work if seeking after it without setting hit-loop stuff
void seek_layout_segmented(VGMSTREAM* vgmstream, int32_t seek_sample) {
    segmented_layout_data* data = vgmstream->layout_data;
    //;VGM_LOG("SEEK [segmented]: seek_sample=%i\n", seek_sample);

    int segment = 0;
    int total_samples = 0;
    while (total_samples < vgmstream->num_samples) {
        int32_t segment_samples = vgmstream_get_samples(data->segments[segment]);

        /* find if sample falls within segment's samples */
        if (seek_sample >= total_samples && seek_sample < total_samples + segment_samples) {
            int32_t seek_relative = seek_sample - total_samples;
            //;VGM_LOG("SEEK [segmented]: found segment=%i, seek_relative=%i (total=%i, target=%i)\n", segment, seek_relative, total_samples, seek_sample);

            seek_vgmstream(data->segments[segment], seek_relative);
            data->current_segment = segment;

            vgmstream->current_sample = seek_sample; 
            vgmstream->samples_into_block = seek_relative; //relative to current segment
            break;
        }

        total_samples += segment_samples;
        segment++;
    }

    // ???
    VGM_ASSERT(segment == data->segment_count, "SEGMENTED: can't find seek segment\n");
}

void loop_layout_segmented(VGMSTREAM* vgmstream, int32_t loop_sample) {
    //;VGM_LOG("SEGMENTED: loop layout at %i\n", loop_sample);
    seek_layout_segmented(vgmstream, loop_sample);
    //;VGM_LOG("SEGMENTED: loop layout done: segment=%i, into=%i\n", data->current_segment, vgmstream->samples_into_block);
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
    if (!data || data->segment_count == 0)
        return false;

    //TODO: simplify, do resample external (use txtp)
    int max_input_channels = 0;
    int max_output_channels = 0;
    sfmt_t max_sample_type = SFMT_NONE;
    int max_sample_rate = data->segments[0]->sample_rate;
    bool mixed_channels = false;
    bool mixed_sample_rate = false;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (int i = 0; i < data->segment_count; i++) {
        VGMSTREAM* vs = data->segments[i];

        if (vs == NULL) {
            VGM_LOG("SEGMENTED: no vgmstream in segment %i\n", i);
            return false;
        }

        if (vs->num_samples <= 0) {
            VGM_LOG("SEGMENTED: no samples in segment %i\n", i);
            return false;
        }

        /* allow config if set for fine-tuned parts (usually TXTP only) */
        vs->config_enabled = vs->config.config_set;

        /* disable so that looping is controlled by render_layout_segmented */
        if (vs->loop_flag) {
            VGM_LOG("SEGMENTED: segment %i is looped\n", i);

            /* config allows internal loops */
            if (!vs->config_enabled) {
                vs->loop_flag = false;
            }
        }

        /* different segments may have different input or output channels (in rare cases of using ex. 2ch + 4ch) */
        int segment_input_channels, segment_output_channels;
        mixing_info(vs, &segment_input_channels, &segment_output_channels);
        if (max_input_channels < segment_input_channels)
            max_input_channels = segment_input_channels;
        if (max_output_channels < segment_output_channels)
            max_output_channels = segment_output_channels;

        if (max_sample_rate < vs->sample_rate) {
            max_sample_rate = vs->sample_rate;
            mixed_sample_rate = true;
        }

        //TODO simplify
        if (i > 0) {
            VGMSTREAM* vs_prev = data->segments[i-1];

            int prev_output_channels;

            mixing_info(vs_prev, NULL, &prev_output_channels);
            if (segment_output_channels != prev_output_channels) {
                mixed_channels = true;
            }

            /* a bit weird, but no matter (should resample) */
            if (vs->sample_rate != vs_prev->sample_rate) {
                VGM_LOG("SEGMENTED: segment %i has different sample rate\n", i);
            }
        }

        sfmt_t current_sample_type = mixing_get_input_sample_type(vs);
        if (max_sample_type < current_sample_type && max_sample_type != SFMT_FLT) //float has priority
            max_sample_type = current_sample_type;

        /* init mixing */
        mixing_setup(vs, VGMSTREAM_SEGMENT_SAMPLE_BUFFER);

        /* final setup in case the VGMSTREAM was created manually */
        setup_vgmstream(vs);
    }

    if (max_output_channels > VGMSTREAM_MAX_CHANNELS || max_input_channels > VGMSTREAM_MAX_CHANNELS)
        return false;

    // needed for codecs like FFMpeg where base vgmstream's sample type is unknown
    data->fmt = max_sample_type;
    int max_sample_size = sfmt_get_sample_size(max_sample_type);

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
