#include "meta.h"
#include "aix_streamfile.h"

/* AIX - interleaved AAX, N segments per channels [SoulCalibur IV (PS3)] */
VGMSTREAM * init_vgmstream_aix(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileAIX = NULL;
    STREAMFILE * streamFileADX = NULL;
    char filename[PATH_LIMIT];
    off_t *segment_offset = NULL;
    int32_t *samples_in_segment = NULL;
    int32_t sample_count;

    int loop_flag = 0;
    int32_t loop_start_sample=0;
    int32_t loop_end_sample=0;

    aix_codec_data *data = NULL;

    off_t first_AIXP;
    off_t stream_list_offset;
    off_t stream_list_end;
    const int segment_list_entry_size = 0x10;
    const off_t segment_list_offset = 0x20;

    int stream_count,channel_count,segment_count;
    int sample_rate;

	int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aix",filename_extension(filename))) goto fail;

    if (read_32bitBE(0x0,streamFile) != 0x41495846 ||   /* "AIXF" */
            read_32bitBE(0x08,streamFile) != 0x01000014 ||
            read_32bitBE(0x0c,streamFile) != 0x00000800)
        goto fail;

    first_AIXP = read_32bitBE(0x4,streamFile)+8;
    segment_count = (uint16_t)read_16bitBE(0x18,streamFile);
    stream_list_offset = segment_list_offset+segment_list_entry_size*segment_count+0x10;

    if (stream_list_offset >= first_AIXP)
        goto fail;
    if (segment_count < 1)
        goto fail;

    sample_rate = read_32bitBE(stream_list_offset+8,streamFile);
    if (sample_rate < 300 || sample_rate > 96000)
        goto fail;

    samples_in_segment = calloc(segment_count,sizeof(int32_t));
    if (!samples_in_segment)
        goto fail;
    segment_offset = calloc(segment_count,sizeof(off_t));
    if (!segment_offset)
        goto fail;

    for (i = 0; i < segment_count; i++)
    {
        segment_offset[i] = read_32bitBE(segment_list_offset+segment_list_entry_size*i+0,streamFile);
        samples_in_segment[i] = read_32bitBE(segment_list_offset+segment_list_entry_size*i+0x08,streamFile);
        /*printf("samples_in_segment[%d]=%d\n",i,samples_in_segment[i]);*/
        /* all segments must have equal sample rate */
        if (read_32bitBE(segment_list_offset+segment_list_entry_size*i+0x0c,streamFile) != sample_rate)
        {
            /* segments > 0 can have 0 sample rate (Ryu ga gotoku: Kenzan! tenkei_sng1.aix),
               seems to indicate same sample rate as first */
            if (!(i > 0 && read_32bitBE(segment_list_offset+segment_list_entry_size*i+0x0c,streamFile) == 0))
                goto fail;
        }
    }

    if (segment_offset[0] != first_AIXP)
        goto fail;

    stream_count = (uint8_t)read_8bit(stream_list_offset,streamFile);
    if (stream_count < 1)
        goto fail;
    stream_list_end = stream_list_offset + 0x8 + stream_count * 8;

    if (stream_list_end >= first_AIXP)
        goto fail;

    channel_count = 0;
    for (i = 0; i < stream_count; i++)
    {
        /* all streams must have same samplerate as segments */
        if (read_32bitBE(stream_list_offset+8+i*8,streamFile)!=sample_rate)
            goto fail;
        channel_count += read_8bit(stream_list_offset+8+i*8+4,streamFile);
    }

    /* check for existence of segments */
    for (i = 0; i < segment_count; i++)
    {
        int j;
        off_t AIXP_offset = segment_offset[i];
        for (j = 0; j < stream_count; j++)
        {
            if (read_32bitBE(AIXP_offset,streamFile)!=0x41495850) /* "AIXP" */
                goto fail;
            if (read_8bit(AIXP_offset+8,streamFile)!=j)
                goto fail;
            AIXP_offset += read_32bitBE(AIXP_offset+4,streamFile)+8;
        }
    }

    /*streamFileAIX = streamFile->open(streamFile,filename,sample_rate*0.0375*2/32*18segment_count);*/
    streamFileAIX = streamFile->open(streamFile,filename,sample_rate*0.1*segment_count);
    if (!streamFileAIX) goto fail;

    data = malloc(sizeof(aix_codec_data));
    if (!data) goto fail;
    data->segment_count = segment_count;
    data->stream_count = stream_count;
    data->adxs = malloc(sizeof(STREAMFILE *)*segment_count*stream_count);
    if (!data->adxs) goto fail;
    for (i=0;i<segment_count*stream_count;i++) {
        data->adxs[i] = NULL;
    }
    data->sample_counts = calloc(segment_count,sizeof(int32_t));
    if (!data->sample_counts) goto fail;
    memcpy(data->sample_counts,samples_in_segment,segment_count*sizeof(int32_t));

    /* for each segment */
    for (i = 0; i < segment_count; i++)
    {
        int j;
        /* for each stream */
        for (j = 0; j < stream_count; j++)
        {
            VGMSTREAM *adx;
            /*printf("try opening segment %d/%d stream %d/%d %x\n",i,segment_count,j,stream_count,segment_offset[i]);*/
            streamFileADX = open_aix_with_STREAMFILE(streamFileAIX,segment_offset[i],j);
            if (!streamFileADX) goto fail;
            adx = data->adxs[i*stream_count+j] = init_vgmstream_adx(streamFileADX);
            if (!adx)
                goto fail;
            close_streamfile(streamFileADX); streamFileADX = NULL;

            if (adx->num_samples != data->sample_counts[i] ||
                    adx->loop_flag != 0)
                goto fail;

            /* save start things so we can restart for seeking/looping */
            /* copy the channels */
            memcpy(adx->start_ch,adx->ch,sizeof(VGMSTREAMCHANNEL)*adx->channels);
            /* copy the whole VGMSTREAM */
            memcpy(adx->start_vgmstream,adx,sizeof(VGMSTREAM));

        }
    }

    if (segment_count > 1)
    {
        loop_flag = 1;
    }

    sample_count = 0;
    for (i = 0; i < segment_count; i++)
    {
        sample_count += data->sample_counts[i];

        if (i == 0)
            loop_start_sample = sample_count;
        if (i == 1)
            loop_end_sample = sample_count;
    }

    vgmstream = allocate_vgmstream(channel_count,loop_flag);

    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = sample_rate;

    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->coding_type = data->adxs[0]->coding_type;
    vgmstream->layout_type = layout_aix;
    vgmstream->meta_type = meta_AIX;

    vgmstream->ch[0].streamfile = streamFileAIX;
    data->current_segment = 0;

    vgmstream->codec_data = data;
    free(segment_offset);
    free(samples_in_segment);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileAIX) close_streamfile(streamFileAIX);
    if (streamFileADX) close_streamfile(streamFileADX);
    if (vgmstream) close_vgmstream(vgmstream);
    if (samples_in_segment) free(samples_in_segment);
    if (segment_offset) free(segment_offset);
    if (data) {
        if (data->adxs)
        {
            int i;
            for (i=0;i<data->segment_count*data->stream_count;i++)
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
