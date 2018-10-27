#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "aax_utf.h"


#define MAX_SEGMENTS 2 /* usually segment0=intro, segment1=loop/main */

/* AAX - segmented ADX [Bayonetta (PS3), Pandora's Tower (Wii), Catherine (X360), Binary Domain (PS3)] */
VGMSTREAM * init_vgmstream_aax(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int is_hca;

    int loop_flag = 0, channel_count = 0;
    int32_t sample_count, loop_start_sample = 0, loop_end_sample = 0;
    int segment_count;

    segmented_layout_data *data = NULL;
    int table_error = 0;
    const long top_offset = 0x00;
    off_t segment_offset[MAX_SEGMENTS];
    size_t segment_size[MAX_SEGMENTS];
    int i;


    /* checks */
    if (!check_extensions(streamFile, "aax"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x40555446) /* "@UTF" */
        goto fail;


    /* get segment count, offsets and sizes */
    {
        struct utf_query_result result;
        long aax_string_table_offset;
        long aax_string_table_size;
        long aax_data_offset;

        result = query_utf_nofail(streamFile, top_offset, NULL, &table_error);
        if (table_error) goto fail;

        segment_count = result.rows;
        if (segment_count > MAX_SEGMENTS) goto fail;

        aax_string_table_offset = top_offset+0x08 + result.string_table_offset;
        aax_data_offset = top_offset+0x08 + result.data_offset;
        aax_string_table_size = aax_data_offset - aax_string_table_offset;

        if (result.name_offset+0x04 > aax_string_table_size)
            goto fail;

        if (read_32bitBE(aax_string_table_offset + result.name_offset, streamFile) == 0x41415800) /* "AAX\0" */
            is_hca = 0;
        else if (read_32bitBE(aax_string_table_offset + result.name_offset, streamFile) == 0x48434100) /* "HCA\0" */
            is_hca = 1;
        else
            goto fail;

        /* get offsets of constituent segments */
        for (i = 0; i < segment_count; i++) {
            struct offset_size_pair offset_size;

            offset_size = query_utf_data(streamFile, top_offset, i, "data", &table_error);
            if (table_error) goto fail;

            segment_offset[i] = aax_data_offset + offset_size.offset;
            segment_size[i] = offset_size.size;
        }
    }

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    for (i = 0; i < segment_count; i++) {
        STREAMFILE* temp_streamFile = setup_subfile_streamfile(streamFile, segment_offset[i],segment_size[i], (is_hca ? "hca" : "adx"));
        if (!temp_streamFile) goto fail;

        data->segments[i] = is_hca ?
                init_vgmstream_hca(temp_streamFile) :
                init_vgmstream_adx(temp_streamFile);

        close_streamfile(temp_streamFile);

        if (!data->segments[i]) goto fail;
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;

    /* get looping and samples */
    sample_count = 0;
    loop_flag = 0;
    for (i = 0; i < segment_count; i++) {
        int segment_loop_flag = query_utf_1byte(streamFile, top_offset, i, "lpflg", &table_error);
        if (table_error) segment_loop_flag = 0;

        if (!loop_flag && segment_loop_flag) {
            loop_start_sample = sample_count;
        }

        sample_count += data->segments[i]->num_samples;

        if (!loop_flag && segment_loop_flag) {
            loop_end_sample = sample_count;
            loop_flag = 1;
        }
    }


    channel_count = data->segments[0]->channels;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->meta_type = meta_AAX;
    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;

    vgmstream->layout_data = data;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}


/* CRI's UTF wrapper around DSP [Sonic Colors sfx (Wii), NiGHTS: Journey of Dreams sfx (Wii)] */
VGMSTREAM * init_vgmstream_utf_dsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t channel_size;

    int loop_flag = 0, channel_count, sample_rate;
    long sample_count;

    int table_error = 0;
    const long top_offset = 0x00;
    long top_data_offset, segment_count;
    long body_offset, body_size;
    long header_offset, header_size;


    /* checks */
    /* files don't have extension, we accept  "" for CLI and .aax for plugins (they aren't exactly .aax though) */
    if (!check_extensions(streamFile, "aax,"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x40555446) /* "@UTF" */
        goto fail;

    /* get segment count, offsets and sizes*/
    {
        struct utf_query_result result;
        long top_string_table_offset;
        long top_string_table_size;
        long name_offset;
       
        result = query_utf_nofail(streamFile, top_offset, NULL, &table_error);
        if (table_error) goto fail;

        segment_count = result.rows;
        if (segment_count != 1) goto fail; /* only simple stuff for now (multisegment not known) */

        top_string_table_offset = top_offset + 8 + result.string_table_offset;
        top_data_offset = top_offset + 8 + result.data_offset;
        top_string_table_size = top_data_offset - top_string_table_offset;

        if (result.name_offset+10 > top_string_table_size) goto fail;

        name_offset = top_string_table_offset + result.name_offset;
        if (read_32bitBE(name_offset+0x00, streamFile) != 0x41445043 || /* "ADPC" */
            read_32bitBE(name_offset+0x04, streamFile) != 0x4D5F5749 || /* "M_WI" */
            read_16bitBE(name_offset+0x08, streamFile) != 0x4900)       /* "I\0" */
            goto fail;
    }

    /* get sizes */
    {
        struct offset_size_pair offset_size;

        offset_size = query_utf_data(streamFile, top_offset, 0, "data", &table_error);
        if (table_error) goto fail;
        body_offset = top_data_offset + offset_size.offset;
        body_size = offset_size.size;

        offset_size = query_utf_data(streamFile, top_offset, 0, "header", &table_error);
        if (table_error) goto fail;
        header_offset = top_data_offset + offset_size.offset;
        header_size = offset_size.size;
    }

    channel_count = query_utf_1byte(streamFile, top_offset, 0, "nch", &table_error);
    sample_count = query_utf_4byte(streamFile, top_offset, 0, "nsmpl", &table_error);
    sample_rate = query_utf_4byte(streamFile, top_offset, 0, "sfreq", &table_error);
    if (table_error) goto fail;
    if (channel_count != 1 && channel_count != 2) goto fail;
    if (header_size != channel_count * 0x60) goto fail;

    start_offset = body_offset;
    channel_size = (body_size+7) / 8 * 8 / channel_count;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;

    vgmstream->meta_type = meta_UTF_DSP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = channel_size;

    dsp_read_coefs_be(vgmstream, streamFile, header_offset+0x1c, 0x60);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
