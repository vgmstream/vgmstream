#include "layout.h"
#include "../vgmstream.h"
#include "../mixing.h"

#define VGMSTREAM_MAX_SEGMENTS 512
#define VGMSTREAM_SEGMENT_SAMPLE_BUFFER 8192


/* Decodes samples for segmented streams.
 * Chains together sequential vgmstreams, for data divided into separate sections or files
 * (like one part for intro and other for loop segments, which may even use different codecs). */
void render_vgmstream_segmented(sample_t * outbuf, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0, loop_samples_skip = 0;
    segmented_layout_data *data = vgmstream->layout_data;
    int use_internal_buffer = 0;


    /* normally uses outbuf directly (faster) but could need internal buffer if downmixing */
    if (vgmstream->channels != data->input_channels) {
        use_internal_buffer = 1;
    }


    while (samples_written < sample_count) {
        int samples_to_do;
        int samples_this_segment = data->segments[data->current_segment]->num_samples;

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            int segment, loop_segment, total_samples;

            /* handle looping by finding loop segment and loop_start inside that segment */
            loop_segment = 0;
            total_samples = 0;
            while (total_samples < vgmstream->num_samples) {
                int32_t segment_samples = data->segments[loop_segment]->num_samples;

                if (vgmstream->loop_sample >= total_samples && vgmstream->loop_sample < total_samples + segment_samples) {
                    loop_samples_skip = vgmstream->loop_sample - total_samples;
                    break; /* loop_start falls within loop_segment's samples */
                }
                total_samples += segment_samples;
                loop_segment++;
            }

            if (loop_segment == data->segment_count) {
                VGM_LOG("segmented_layout: can't find loop segment\n");
                loop_segment = 0;
            }

            data->current_segment = loop_segment;

            /* loops can span multiple segments */
            for (segment = loop_segment; segment < data->segment_count; segment++) {
                reset_vgmstream(data->segments[segment]);
            }

            vgmstream->samples_into_block = 0;
            continue;
        }

        samples_to_do = vgmstream_samples_to_do(samples_this_segment, sample_count, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;
        if (samples_to_do > VGMSTREAM_SEGMENT_SAMPLE_BUFFER /*&& use_internal_buffer*/) /* always for fade/etc mixes */
            samples_to_do = VGMSTREAM_SEGMENT_SAMPLE_BUFFER;

        /* segment looping: discard until actual start */
        if (loop_samples_skip > 0) {
            if (samples_to_do > loop_samples_skip)
                samples_to_do = loop_samples_skip;
        }

        /* detect segment change and restart */
        if (samples_to_do == 0) {
            data->current_segment++;
            reset_vgmstream(data->segments[data->current_segment]);
            vgmstream->samples_into_block = 0;
            continue;
        }

        render_vgmstream(
                use_internal_buffer ?
                        data->buffer :
                        &outbuf[samples_written * data->output_channels],
                samples_to_do,
                data->segments[data->current_segment]);

        if (loop_samples_skip > 0) {
            loop_samples_skip -= samples_to_do;
            vgmstream->samples_into_block += samples_to_do;
            continue;
        }

        if (use_internal_buffer) {
            int s;
            for (s = 0; s < samples_to_do * data->output_channels; s++) {
                outbuf[samples_written * data->output_channels + s] = data->buffer[s];
            }
        }

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }
}


segmented_layout_data* init_layout_segmented(int segment_count) {
    segmented_layout_data *data = NULL;

    if (segment_count <= 0 || segment_count > VGMSTREAM_MAX_SEGMENTS)
        goto fail;

    data = calloc(1, sizeof(segmented_layout_data));
    if (!data) goto fail;

    data->segment_count = segment_count;
    data->current_segment = 0;

    data->segments = calloc(segment_count, sizeof(VGMSTREAM*));
    if (!data->segments) goto fail;

    return data;
fail:
    free_layout_segmented(data);
    return NULL;
}

int setup_layout_segmented(segmented_layout_data* data) {
    int i, max_input_channels = 0, max_output_channels = 0;
    sample_t *outbuf_re = NULL;


    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (i = 0; i < data->segment_count; i++) {
        int segment_input_channels, segment_output_channels;

        if (data->segments[i] == NULL) {
            VGM_LOG("segmented: no vgmstream in segment %i\n", i);
            goto fail;
        }


        if (data->segments[i]->num_samples <= 0) {
            VGM_LOG("segmented: no samples in segment %i\n", i);
            goto fail;
        }


        /* disable so that looping is controlled by render_vgmstream_segmented */
        if (data->segments[i]->loop_flag != 0) {
            VGM_LOG("segmented: segment %i is looped\n", i);
            data->segments[i]->loop_flag = 0;
        }

        /* different segments may have different input channels, though output should be
         * the same for all (ex. 2ch + 1ch segments, but 2ch segment is downmixed to 1ch) */
        mixing_info(data->segments[i], &segment_input_channels, &segment_output_channels);
        if (max_input_channels < segment_input_channels)
            max_input_channels = segment_input_channels;
        if (max_output_channels < segment_output_channels)
            max_output_channels = segment_output_channels;

        if (i > 0) {
            int prev_output_channels;

            mixing_info(data->segments[i-1], NULL, &prev_output_channels);
            if (segment_output_channels != prev_output_channels) {
                VGM_LOG("segmented: segment %i has wrong channels %i vs prev channels %i\n", i, segment_output_channels, prev_output_channels);
                goto fail;
            }

            /* a bit weird, but no matter */
            if (data->segments[i]->sample_rate != data->segments[i-1]->sample_rate) {
                VGM_LOG("segmented: segment %i has different sample rate\n", i);
            }

            /* perfectly acceptable */
            //if (data->segments[i]->coding_type != data->segments[i-1]->coding_type)
            //    goto fail;
        }


        setup_vgmstream(data->segments[i]); /* final setup in case the VGMSTREAM was created manually */

        mixing_setup(data->segments[i], VGMSTREAM_SEGMENT_SAMPLE_BUFFER); /* init mixing */
    }

    if (max_output_channels > VGMSTREAM_MAX_CHANNELS || max_input_channels > VGMSTREAM_MAX_CHANNELS)
        goto fail;

    /* create internal buffer big enough for mixing */
    outbuf_re = realloc(data->buffer, VGMSTREAM_SEGMENT_SAMPLE_BUFFER*max_input_channels*sizeof(sample_t));
    if (!outbuf_re) goto fail;
    data->buffer = outbuf_re;

    data->input_channels = max_input_channels;
    data->output_channels = max_output_channels;

    return 1;
fail:
    return 0; /* caller is expected to free */
}

void free_layout_segmented(segmented_layout_data *data) {
    int i, j;

    if (!data)
        return;

    if (data->segments) {
        for (i = 0; i < data->segment_count; i++) {
            int is_repeat = 0;

            /* segments are allowed to be repeated so don't close the same thing twice */
            for (j = 0; j < i; j++) {
                if (data->segments[i] == data->segments[j])
                    is_repeat = 1;
            }
            if (is_repeat)
                continue;

            close_vgmstream(data->segments[i]);
        }
        free(data->segments);
    }
    free(data);
}

void reset_layout_segmented(segmented_layout_data *data) {
    int i;

    if (!data)
        return;

    data->current_segment = 0;
    for (i = 0; i < data->segment_count; i++) {
        reset_vgmstream(data->segments[i]);
    }
}

/* helper for easier creation of segments */
VGMSTREAM *allocate_segmented_vgmstream(segmented_layout_data* data, int loop_flag, int loop_start_segment, int loop_end_segment) {
    VGMSTREAM *vgmstream = NULL;
    int channel_layout;
    int i, num_samples, loop_start, loop_end;

    /* save data */
    channel_layout = data->segments[0]->channel_layout;
    num_samples = 0;
    loop_start = 0;
    loop_end = 0;
    for (i = 0; i < data->segment_count; i++) {
        if (loop_flag && i == loop_start_segment)
            loop_start = num_samples;

        num_samples += data->segments[i]->num_samples;

        if (loop_flag && i == loop_end_segment)
            loop_end = num_samples;

        /* inherit first segment's layout but only if all segments' layout match */
        if (channel_layout != 0 && channel_layout != data->segments[i]->channel_layout)
            channel_layout = 0;
    }

    /* respect loop_flag even when no loop_end found as it's possible file loops are set outside */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(data->output_channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = data->segments[0]->meta_type;
    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->channel_layout = channel_layout;

    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    if (vgmstream) vgmstream->layout_data = NULL;
    close_vgmstream(vgmstream);
    return NULL;
}
