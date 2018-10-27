#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* Nippon Ichi SPS wrapper (segmented)  [Penny-Punching Princess (Switch)] */
VGMSTREAM * init_vgmstream_opus_sps_n1_segmented(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t segment_offset;
    int loop_flag, channel_count;
    int i;

    segmented_layout_data *data = NULL;
    int segment_count, loop_start_segment = 0, loop_end_segment = 0;
    int num_samples = 0, loop_start_sample = 0, loop_end_sample = 0;


    /* checks */
    if (!check_extensions(streamFile, "at9"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x09000000) /* file type (see other N1 SPS) */
        goto fail;
    if (read_32bitLE(0x04,streamFile) + 0x1c != get_streamfile_size(streamFile))
        goto fail;
    /* 0x08(2): sample rate, 0x0a(2): flag?, 0x0c: num_samples (slightly smaller than added samples) */

    segment_count = 3; /* intro/loop/end */
    loop_start_segment = 1;
    loop_end_segment = 1;
    loop_flag = (segment_count > 0);

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    segment_offset = 0x1c;
    for (i = 0; i < segment_count; i++) {
        STREAMFILE* temp_streamFile;
        size_t segment_size = read_32bitLE(0x10+0x04*i,streamFile);

        if (!segment_size)
            goto fail;

        temp_streamFile = setup_subfile_streamfile(streamFile, segment_offset,segment_size, "opus");
        if (!temp_streamFile) goto fail;

        data->segments[i] = init_vgmstream_opus_std(temp_streamFile);
        close_streamfile(temp_streamFile);
        if (!data->segments[i]) goto fail;

        segment_offset += segment_size;

        //todo there are some trailing samples that must be removed for smooth loops, start skip seems ok
        data->segments[i]->num_samples -= 374; //not correct for all files, no idea how to calculate

        /* get looping and samples */
        if (loop_flag && loop_start_segment == i)
            loop_start_sample = num_samples;

        num_samples += data->segments[i]->num_samples;

        if (loop_flag && loop_end_segment == i)
            loop_end_sample = num_samples;
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;


    channel_count = data->segments[0]->channels;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x08,streamFile);
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->meta_type = meta_OPUS_PPP;
    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}
