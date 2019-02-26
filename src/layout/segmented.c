#include "layout.h"
#include "../vgmstream.h"


#define VGMSTREAM_MAX_SEGMENTS 255


/* Decodes samples for segmented streams.
 * Chains together sequential vgmstreams, for data divided into separate sections or files
 * (like one part for intro and other for loop segments, which may even use different codecs). */
void render_vgmstream_segmented(sample_t * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0, loop_samples_skip = 0;
    segmented_layout_data *data = vgmstream->layout_data;


    while (samples_written < sample_count) {
        int samples_to_do;
        int samples_this_block = data->segments[data->current_segment]->num_samples;


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

        samples_to_do = vgmstream_samples_to_do(samples_this_block, sample_count, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

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

        render_vgmstream(&buffer[samples_written*data->segments[data->current_segment]->channels],
                samples_to_do,data->segments[data->current_segment]);

        if (loop_samples_skip > 0) {
            loop_samples_skip -= samples_to_do;
            vgmstream->samples_into_block += samples_to_do;
        }
        else {
            samples_written += samples_to_do;
            vgmstream->current_sample += samples_to_do;
            vgmstream->samples_into_block += samples_to_do;
        }
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
    int i;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (i = 0; i < data->segment_count; i++) {
        if (!data->segments[i])
            goto fail;

        if (data->segments[i]->num_samples <= 0)
            goto fail;

        /* shouldn't happen */
        if (data->segments[i]->loop_flag != 0) {
            VGM_LOG("segmented layout: segment %i is looped\n", i);
            data->segments[i]->loop_flag = 0;
        }

        if (i > 0) {
            if (data->segments[i]->channels != data->segments[i-1]->channels)
                goto fail;

            /* a bit weird, but no matter */
            if (data->segments[i]->sample_rate != data->segments[i-1]->sample_rate) {
                VGM_LOG("segmented layout: segment %i has different sample rate\n", i);
            }

            //if (data->segments[i]->coding_type != data->segments[i-1]->coding_type)
            //    goto fail; /* perfectly acceptable */
        }


        setup_vgmstream(data->segments[i]); /* final setup in case the VGMSTREAM was created manually */
    }


    return 1;
fail:
    return 0; /* caller is expected to free */
}

void free_layout_segmented(segmented_layout_data *data) {
    int i;

    if (!data)
        return;

    if (data->segments) {
        for (i = 0; i < data->segment_count; i++) {
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
    VGMSTREAM *vgmstream;
    int i, num_samples, loop_start, loop_end;

    /* get data */
    num_samples = 0;
    loop_start = 0;
    loop_end = 0;
    for (i = 0; i < data->segment_count; i++) {
        if (loop_flag && i == loop_start_segment)
            loop_start = num_samples;

        num_samples += data->segments[i]->num_samples;

        if (loop_flag && i == loop_end_segment)
            loop_end = num_samples;
    }

    /* respect loop_flag even when no loop_end found as it's possible file loops are set outside */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(data->segments[0]->channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = data->segments[0]->meta_type;
    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->coding_type = data->segments[0]->coding_type;

    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    if (vgmstream) vgmstream->layout_data = NULL;
    close_vgmstream(vgmstream);
    return NULL;
}
