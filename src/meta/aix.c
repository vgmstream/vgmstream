#include "meta.h"
#include "aix_streamfile.h"

/* AIX - interleaved AAX, N segments with M layers (1/2ch) inside [SoulCalibur IV (PS3), Dragon Ball Z: Burst Limit (PS3)] */
VGMSTREAM * init_vgmstream_aix(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileAIX = NULL;
    int loop_flag = 0, channel_count, sample_rate;
    int32_t sample_count, loop_start_sample = 0, loop_end_sample = 0;

    off_t *segment_offset = NULL;
    int32_t *segment_samples = NULL;
    aix_codec_data *data = NULL;
    off_t data_offset;
    off_t layer_list_offset;
    off_t layer_list_end;
    const off_t segment_list_offset = 0x20;
    const size_t segment_list_entry_size = 0x10;
    const size_t layer_list_entry_size = 0x08;

    int segment_count, layer_count;
    int i, j;


    /* checks */
    if (!check_extensions(streamFile, "aix"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41495846 ||  /* "AIXF" */
        read_32bitBE(0x08,streamFile) != 0x01000014 ||  /* version? */
        read_32bitBE(0x0c,streamFile) != 0x00000800)
        goto fail;

    /* base segment header */
    data_offset = read_32bitBE(0x04,streamFile)+0x08;

    segment_count = (uint16_t)read_16bitBE(0x18,streamFile);
    if (segment_count < 1) goto fail;

    layer_list_offset = segment_list_offset + segment_count*segment_list_entry_size + 0x10;
    if (layer_list_offset >= data_offset) goto fail;

    segment_samples = calloc(segment_count,sizeof(int32_t));
    if (!segment_samples) goto fail;
    segment_offset = calloc(segment_count,sizeof(off_t));
    if (!segment_offset) goto fail;

    /* parse segments table */
    {
        sample_rate = read_32bitBE(layer_list_offset+0x08,streamFile); /* first layer's sample rate */

        for (i = 0; i < segment_count; i++) {
            segment_offset[i]  = read_32bitBE(segment_list_offset + segment_list_entry_size*i + 0x00,streamFile);
            /* 0x04: segment size */
            segment_samples[i] = read_32bitBE(segment_list_offset + segment_list_entry_size*i + 0x08,streamFile);

            /* all segments must have equal sample rate */
            if (read_32bitBE(segment_list_offset + segment_list_entry_size*i + 0x0c,streamFile) != sample_rate) {
                /* segments > 0 can have 0 sample rate (Ryu ga gotoku: Kenzan! tenkei_sng1.aix),
                   seems to indicate same sample rate as first */
                if (!(i > 0 && read_32bitBE(segment_list_offset + segment_list_entry_size*i + 0x0c,streamFile) == 0))
                    goto fail;
            }
        }

        if (segment_offset[0] != data_offset)
            goto fail;
    }

    /* base layer header */
    layer_count = (uint8_t)read_8bit(layer_list_offset,streamFile);
    if (layer_count < 1) goto fail;

    layer_list_end = layer_list_offset + 0x08 + layer_count*layer_list_entry_size;
    if (layer_list_end >= data_offset) goto fail;

    /* parse layers table */
    channel_count = 0;
    for (i = 0; i < layer_count; i++) {
        /* all streams must have same samplerate as segments */
        if (read_32bitBE(layer_list_offset + 0x08 + i*layer_list_entry_size + 0x00,streamFile) != sample_rate)
            goto fail;
        channel_count += read_8bit(layer_list_offset + 0x08 + i*layer_list_entry_size + 0x04,streamFile);
    }

    /* check for existence of segments */
    for (i = 0; i < segment_count; i++) {
        off_t aixp_offset = segment_offset[i];
        for (j = 0; j < layer_count; j++) {
            if (read_32bitBE(aixp_offset,streamFile) != 0x41495850) /* "AIXP" */
                goto fail;
            if (read_8bit(aixp_offset+0x08,streamFile) != j)
                goto fail;
            aixp_offset += read_32bitBE(aixp_offset+0x04,streamFile) + 0x08;
        }
    }

    /* open base streamfile, that will be shared by all open_aix_with_STREAMFILE */
    {
        char filename[PATH_LIMIT];

        streamFile->get_name(streamFile,filename,sizeof(filename));
        streamFileAIX = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE); //todo simplify
        if (!streamFileAIX) goto fail;
    }

    /* init layout */
    {
        data = malloc(sizeof(aix_codec_data));
        if (!data) goto fail;

        data->segment_count = segment_count;
        data->stream_count = layer_count;
        data->adxs = calloc(segment_count * layer_count, sizeof(VGMSTREAM*));
        if (!data->adxs) goto fail;

        data->sample_counts = calloc(segment_count,sizeof(int32_t));
        if (!data->sample_counts) goto fail;

        memcpy(data->sample_counts,segment_samples,segment_count*sizeof(int32_t));
    }

    /* open each segment / layer subfile */
    for (i = 0; i < segment_count; i++) {
        for (j = 0; j < layer_count; j++) {
            //;VGM_LOG("AIX: opening segment %d/%d stream %d/%d %x\n",i,segment_count,j,stream_count,segment_offset[i]);
            VGMSTREAM *temp_vgmstream;
            STREAMFILE * temp_streamFile = open_aix_with_STREAMFILE(streamFileAIX,segment_offset[i],j);
            if (!temp_streamFile) goto fail;

            temp_vgmstream = data->adxs[i*layer_count+j] = init_vgmstream_adx(temp_streamFile);

            close_streamfile(temp_streamFile);

            if (!temp_vgmstream) goto fail;

            /* setup layers */
            if (temp_vgmstream->num_samples != data->sample_counts[i] || temp_vgmstream->loop_flag != 0)
                goto fail;
            memcpy(temp_vgmstream->start_ch,temp_vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*temp_vgmstream->channels);
            memcpy(temp_vgmstream->start_vgmstream,temp_vgmstream,sizeof(VGMSTREAM));
        }
    }

    /* get looping and samples */
    sample_count = 0;
    loop_flag = (segment_count > 1);
    for (i = 0; i < segment_count; i++) {
        sample_count += data->sample_counts[i];

        if (i == 0)
            loop_start_sample = sample_count;
        if (i == 1)
            loop_end_sample = sample_count;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->meta_type = meta_AIX;
    vgmstream->coding_type = data->adxs[0]->coding_type;
    vgmstream->layout_type = layout_aix;

    vgmstream->ch[0].streamfile = streamFileAIX;
    data->current_segment = 0;

    vgmstream->codec_data = data;
    free(segment_offset);
    free(segment_samples);

    return vgmstream;

fail:
    close_streamfile(streamFileAIX);
    close_vgmstream(vgmstream);
    free(segment_samples);
    free(segment_offset);

    /* free aix layout */
    if (data) {
        if (data->adxs) {
            int i;
            for (i = 0; i < data->segment_count*data->stream_count; i++) {
                close_vgmstream(data->adxs[i]);
            }
            free(data->adxs);
        }
        if (data->sample_counts) {
            free(data->sample_counts);
        }
        free(data);
    }
    return NULL;
}
