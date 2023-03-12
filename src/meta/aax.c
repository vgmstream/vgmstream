#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/cri_utf.h"


#define MAX_SEGMENTS 2 /* usually segment0=intro, segment1=loop/main */

/* AAX - segmented ADX [Bayonetta (PS3), Pandora's Tower (Wii), Catherine (X360), Binary Domain (PS3)] */
VGMSTREAM* init_vgmstream_aax(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag = 0, channels = 0;
    int32_t sample_count, loop_start_sample = 0, loop_end_sample = 0;

    segmented_layout_data* data = NULL;
    int segment_count, loop_segment = 0, is_hca;
    off_t segment_offset[MAX_SEGMENTS];
    size_t segment_size[MAX_SEGMENTS];
    int i;
    utf_context* utf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;

    /* .aax: often with extension (with either HCA or AAX tables)
     * (extensionless): sometimes without [PES 2013 (PC)] */
    if (!check_extensions(sf, "aax,"))
        goto fail;

    /* .aax contains a simple UTF table, each row being a segment pointing to a CRI audio format */
    {
        int rows;
        const char* name;
        uint32_t table_offset = 0x00;


        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "AAX") == 0)
            is_hca = 0;
        else if (strcmp(name, "HCA") == 0)
            is_hca = 1;
        else
            goto fail;

        segment_count = rows;
        if (segment_count > MAX_SEGMENTS) goto fail;


        /* get offsets of constituent segments */
        for (i = 0; i < segment_count; i++) {
            uint32_t offset, size;
            uint8_t segment_loop_flag = 0;

            if (!utf_query_u8(utf, i, "lpflg", &segment_loop_flag)) /* usually in last segment */
                goto fail;
            if (!utf_query_data(utf, i, "data", &offset, &size))
                goto fail;

            segment_offset[i] = table_offset + offset;
            segment_size[i] = size;
            if (segment_loop_flag) {
                loop_flag = 1;
                loop_segment = i;
            }
        }
    }

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    for (i = 0; i < segment_count; i++) {
        STREAMFILE* temp_sf = setup_subfile_streamfile(sf, segment_offset[i],segment_size[i], (is_hca ? "hca" : "adx"));
        if (!temp_sf) goto fail;

        data->segments[i] = is_hca ?
                init_vgmstream_hca(temp_sf) :
                init_vgmstream_adx(temp_sf);

        close_streamfile(temp_sf);

        if (!data->segments[i]) goto fail;
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;

    /* get looping and samples */
    sample_count = 0;
    for (i = 0; i < segment_count; i++) {

        if (loop_flag && loop_segment == i) {
            loop_start_sample = sample_count;
        }

        sample_count += data->segments[i]->num_samples;

        if (loop_flag && loop_segment == i) {
            loop_end_sample = sample_count;
        }
    }

    channels = data->output_channels;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->meta_type = meta_AAX;
    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;

    vgmstream->layout_data = data;

    utf_close(utf);
    return vgmstream;

fail:
    utf_close(utf);
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}
