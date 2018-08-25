#include "layout.h"
#include "../vgmstream.h"


/* Decodes samples for segmented streams.
 * Chains together sequential vgmstreams, for data divided into separate sections or files
 * (like one part for intro and other for loop segments, which may even use different codecs). */
void render_vgmstream_segmented(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0;
    segmented_layout_data *data = vgmstream->layout_data;


    while (samples_written < sample_count) {
        int samples_to_do;
        int samples_this_block = data->segments[data->current_segment]->num_samples;


        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* handle looping, finding loop segment */
            int loop_segment = 0, samples = 0, loop_samples_skip = 0;
            while (samples < vgmstream->num_samples) {
                int32_t segment_samples = data->segments[loop_segment]->num_samples;
                if (vgmstream->loop_start_sample >= samples && vgmstream->loop_start_sample < samples + segment_samples) {
                    loop_samples_skip = vgmstream->loop_start_sample - samples;
                    break; /* loop_start falls within loop_segment's samples */
                }
                samples += segment_samples;
                loop_segment++;
            }
            if (loop_segment == data->segment_count) {
                VGM_LOG("segmented_layout: can't find loop segment\n");
                loop_segment = 0;
            }
            if (loop_samples_skip > 0) {
                VGM_LOG("segmented_layout: loop starts after %i samples\n", loop_samples_skip);
                //todo skip/fix, but probably won't happen
            }

            data->current_segment = loop_segment;
            reset_vgmstream(data->segments[data->current_segment]);
            vgmstream->samples_into_block = 0;
            continue;
        }

        samples_to_do = vgmstream_samples_to_do(samples_this_block, sample_count, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        /* detect segment change and restart */
        if (samples_to_do == 0) {
            data->current_segment++;
            reset_vgmstream(data->segments[data->current_segment]);
            vgmstream->samples_into_block = 0;
            continue;
        }

        render_vgmstream(&buffer[samples_written*data->segments[data->current_segment]->channels],
                samples_to_do,data->segments[data->current_segment]);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }
}


segmented_layout_data* init_layout_segmented(int segment_count) {
    segmented_layout_data *data = NULL;

    if (segment_count <= 0 || segment_count > 255)
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


        /* save start things so we can restart for seeking/looping */
        memcpy(data->segments[i]->start_ch,data->segments[i]->ch,sizeof(VGMSTREAMCHANNEL)*data->segments[i]->channels);
        memcpy(data->segments[i]->start_vgmstream,data->segments[i],sizeof(VGMSTREAM));
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
