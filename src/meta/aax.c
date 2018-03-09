#include "meta.h"
#include "aax_streamfile.h"
#include "aax_utf.h"


/* AAX - segmented ADX [Bayonetta (PS3), Pandora's Tower (Wii), Catherine (X360)] */
VGMSTREAM * init_vgmstream_aax(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileAAX = NULL;
    STREAMFILE * streamFileADX = NULL;
    char filename[PATH_LIMIT];
    off_t *segment_offset = NULL;
    off_t *segment_size = NULL;
    int32_t sample_count;
    int table_error = 0;

    int loop_flag = 0;
    int32_t loop_start_sample=0;
    int32_t loop_end_sample=0;
    int loop_segment = 0;

    aax_codec_data *data = NULL;

    const long AAX_offset = 0;

    int channel_count = 0, segment_count;
    int sample_rate = 0;

    int i;


    long aax_data_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aax",filename_extension(filename))) goto fail;
    
    /* get AAX entry count, data offset */
    {
        struct utf_query_result result;
        long aax_string_table_offset;
        long aax_string_table_size;
       
        result = query_utf_nofail(streamFile, AAX_offset, NULL, &table_error);
        if (table_error) goto fail;
        segment_count = result.rows;
        aax_string_table_offset = AAX_offset + 8 + result.string_table_offset;
        aax_data_offset = AAX_offset + 8 + result.data_offset;
        aax_string_table_size = aax_data_offset - aax_string_table_offset;

        if (result.name_offset+4 > aax_string_table_size) goto fail;
        if (read_32bitBE(aax_string_table_offset + result.name_offset,
                    streamFile) != 0x41415800) /* "AAX\0" */
            goto fail;
    }

    segment_offset = calloc(segment_count,sizeof(off_t));
    if (!segment_offset)
        goto fail;
    segment_size = calloc(segment_count,sizeof(off_t));
    if (!segment_size)
        goto fail;

    /* get offsets of constituent ADXs */
    for (i = 0; i < segment_count; i++)
    {
        struct offset_size_pair offset_size;
        offset_size = query_utf_data(streamFile, AAX_offset, i, "data", &table_error);
        if (table_error) goto fail;
        segment_offset[i] = aax_data_offset + offset_size.offset;
        segment_size[i] = offset_size.size;
    }

    streamFileAAX = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!streamFileAAX) goto fail;

    data = malloc(sizeof(aax_codec_data));
    if (!data) goto fail;
    data->segment_count = segment_count;
    data->adxs = malloc(sizeof(STREAMFILE *)*segment_count);
    if (!data->adxs) goto fail;
    for (i=0;i<segment_count;i++) {
        data->adxs[i] = NULL;
    }
    data->sample_counts = calloc(segment_count,sizeof(int32_t));
    if (!data->sample_counts) goto fail;

    /* for each segment */
    for (i = 0; i < segment_count; i++)
    {
        VGMSTREAM *adx;
        /*printf("try opening segment %d/%d %x\n",i,segment_count,segment_offset[i]);*/
        streamFileADX = open_aax_with_STREAMFILE(streamFileAAX,segment_offset[i],segment_size[i]);
        if (!streamFileADX) goto fail;
        adx = data->adxs[i] = init_vgmstream_adx(streamFileADX);
        if (!adx)
            goto fail;
        data->sample_counts[i] = adx->num_samples;
        close_streamfile(streamFileADX); streamFileADX = NULL;

        if (i == 0)
        {
            channel_count = adx->channels;
            sample_rate = adx->sample_rate;
        }
        else
        {
            if (channel_count != adx->channels)
                goto fail;
            if (sample_rate != adx->sample_rate)
                goto fail;
        }

        if (adx->loop_flag != 0)
            goto fail;

        /* save start things so we can restart for seeking/looping */
        /* copy the channels */
        memcpy(adx->start_ch,adx->ch,sizeof(VGMSTREAMCHANNEL)*adx->channels);
        /* copy the whole VGMSTREAM */
        memcpy(adx->start_vgmstream,adx,sizeof(VGMSTREAM));

    }

    sample_count = 0;
    loop_flag = 0;
    for (i = 0; i < segment_count; i++)
    {
        int segment_loop_flag = query_utf_1byte(streamFile, AAX_offset, i,
                "lpflg", &table_error);
        if (table_error) segment_loop_flag = 0;

        if (!loop_flag && segment_loop_flag)
        {
            loop_start_sample = sample_count;
            loop_segment = i;
        }

        sample_count += data->sample_counts[i];

        if (!loop_flag && segment_loop_flag)
        {
            loop_end_sample = sample_count;
            loop_flag = 1;
        }
    }

    vgmstream = allocate_vgmstream(channel_count,loop_flag);

    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = sample_rate;

    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = data->adxs[0]->coding_type;
    vgmstream->layout_type = layout_aax;
    vgmstream->meta_type = meta_AAX;

    vgmstream->ch[0].streamfile = streamFileAAX;
    data->current_segment = 0;
    data->loop_segment = loop_segment;

    vgmstream->codec_data = data;
    free(segment_offset);
    free(segment_size);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileAAX) close_streamfile(streamFileAAX);
    if (streamFileADX) close_streamfile(streamFileADX);
    if (vgmstream) close_vgmstream(vgmstream);
    if (segment_offset) free(segment_offset);
    if (segment_size) free(segment_size);
    if (data) {
        if (data->adxs)
        {
            int i;
            for (i=0;i<data->segment_count;i++)
                if (data->adxs)
                    close_vgmstream(data->adxs[i]);
            free(data->adxs);
        }
        if (data->sample_counts)
        {
            free(data->sample_counts);
        }
        free(data);
    }
    return NULL;
}


/* CRI's UTF wrapper around DSP */
VGMSTREAM * init_vgmstream_utf_dsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int table_error = 0;

    int loop_flag = 0;

    const long top_offset = 0;

    int channel_count;
    int sample_rate;
    long sample_count;

    long top_data_offset, segment_count;
    long body_offset, body_size;
    long header_offset, header_size;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    //if (strcasecmp("aax",filename_extension(filename))) goto fail;

    /* get entry count, data offset */
    {
        struct utf_query_result result;
        long top_string_table_offset;
        long top_string_table_size;
        long name_offset;
       
        result = query_utf_nofail(streamFile, top_offset, NULL, &table_error);
        if (table_error) goto fail;
        segment_count = result.rows;
        if (segment_count != 1) goto fail; // only simple stuff for now
        top_string_table_offset = top_offset + 8 + result.string_table_offset;
        top_data_offset = top_offset + 8 + result.data_offset;
        top_string_table_size = top_data_offset - top_string_table_offset;

        if (result.name_offset+10 > top_string_table_size) goto fail;

        name_offset = top_string_table_offset + result.name_offset;
        if (read_32bitBE(name_offset, streamFile) != 0x41445043   ||// "ADPC"
            read_32bitBE(name_offset+4, streamFile) != 0x4D5F5749 ||// "M_WI"
            read_16bitBE(name_offset+8, streamFile) != 0x4900)      // "I\0"
            goto fail;
    }

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

    vgmstream = allocate_vgmstream(channel_count,loop_flag);

    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_UTF_DSP;

    {
        int i,j;
        long channel_size = (body_size+7)/8*8/channel_count;
        for (i = 0; i < channel_count; i++)
        {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!vgmstream->ch[i].streamfile) goto fail;
            vgmstream->ch[i].channel_start_offset =
                vgmstream->ch[i].offset = body_offset + i * channel_size;
            for (j=0;j<16;j++)
            {
                vgmstream->ch[i].adpcm_coef[j] =
                    read_16bitBE(header_offset + 0x60*i + 0x1c + j*2, streamFile);
            }
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
