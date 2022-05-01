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


/* CRI's UTF wrapper around DSP [Sonic Colors sfx (Wii), NiGHTS: Journey of Dreams sfx (Wii)] */
VGMSTREAM* init_vgmstream_utf_dsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint8_t loop_flag = 0, channels;
    uint32_t sample_rate, num_samples, loop_start, loop_end, interleave;
    uint32_t data_offset, data_size,  header_offset, header_size;
    utf_context* utf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;

    /* .aax: assumed
     * (extensionless): extracted names inside csb/cpk often don't have extensions */
    if (!check_extensions(sf, "aax,"))
        goto fail;

    /* .aax contains a simple UTF table with one row and various columns being header info */
    {
        int rows;
        const char* name;
        uint32_t table_offset = 0x00;


        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "ADPCM_WII") != 0)
            goto fail;

        if (rows != 1)
            goto fail;

        if (!utf_query_u32(utf, 0, "sfreq", &sample_rate))
            goto fail;
        if (!utf_query_u32(utf, 0, "nsmpl", &num_samples))
            goto fail;
        if (!utf_query_u8(utf, 0, "nch", &channels))
            goto fail;
        if (!utf_query_u8(utf, 0, "lpflg", &loop_flag)) /* full loops */
            goto fail;
        /* for some reason data is stored before header */
        if (!utf_query_data(utf, 0, "data", &data_offset, &data_size))
            goto fail;
        if (!utf_query_data(utf, 0, "header", &header_offset, &header_size))
            goto fail;

        if (channels < 1 || channels > 2)
            goto fail;
        if (header_size != channels * 0x60)
            goto fail;

        start_offset = data_offset;
        interleave = (data_size+7) / 8 * 8 / channels;

        loop_start = read_32bitBE(header_offset + 0x10, sf);
        loop_end   = read_32bitBE(header_offset + 0x14, sf);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(loop_start);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(loop_end) + 1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_UTF_DSP;

    dsp_read_coefs_be(vgmstream, sf, header_offset+0x1c, 0x60);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
