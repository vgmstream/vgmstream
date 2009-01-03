#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

typedef struct _AAXSTREAMFILE
{
  STREAMFILE sf;
  STREAMFILE *real_file;
  off_t start_physical_offset;
  size_t file_size;
} AAXSTREAMFILE;

static STREAMFILE *open_aax_with_STREAMFILE(STREAMFILE *file,off_t start_offset,size_t file_size);

VGMSTREAM * init_vgmstream_aax(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileAAX = NULL;
    STREAMFILE * streamFileADX = NULL;
    char filename[260];
    off_t *segment_offset = NULL;
    int32_t sample_count;

    int loop_flag = 0;
    int32_t loop_start_sample=0;
    int32_t loop_end_sample=0;

    aax_codec_data *data = NULL;

    int channel_count,segment_count;
    int sample_rate;

	int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aax",filename_extension(filename))) goto fail;
    
    /* TODO: do file header check */

    segment_count = 2; /* TODO: segment_count */

    segment_offset = calloc(segment_count,sizeof(off_t));
    if (!segment_offset)
        goto fail;

    /* TODO: segment_offset */
    segment_offset[0] = 0x58 + 0;
    segment_offset[1] = 0x58 + 0x000eeae8;
#if 0
    for (i = 0; i < segment_count; i++)
    {
        segment_offset[i] = 
    }
#endif

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
        /* TODO: segment size */
        size_t segment_size[2] = {0x000eeae6, 0x006cc41e};
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
    for (i = 0; i < segment_count; i++)
    {
        sample_count += data->sample_counts[i];

        /* TODO: check loop flag, set loop flag for AAX */
        loop_flag = 1;
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
    vgmstream->layout_type = layout_aax;
    vgmstream->meta_type = meta_AAX;

    vgmstream->ch[0].streamfile = streamFileAAX;
    data->current_segment = 0;
    data->loop_segment = 1; /*TODO: loop segment */;

    vgmstream->codec_data = data;
    free(segment_offset);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileAAX) close_streamfile(streamFileAAX);
    if (streamFileADX) close_streamfile(streamFileADX);
    if (vgmstream) close_vgmstream(vgmstream);
    if (segment_offset) free(segment_offset);
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
static size_t read_aax(AAXSTREAMFILE *streamfile,uint8_t *dest,off_t offset,size_t length)
{
  /* truncate at end of logical file */
  if (streamfile->start_physical_offset+offset+length > streamfile->file_size)
  {
      long signed_length = length;
      signed_length = streamfile->file_size - (streamfile->start_physical_offset+offset);
      if (signed_length < 0) signed_length = 0;
      length = signed_length;
  }
  return read_streamfile(dest,
          streamfile->start_physical_offset+offset,
          length,streamfile->real_file);
}

static void close_aax(AAXSTREAMFILE *streamfile)
{
    free(streamfile);
    return;
}

static size_t get_size_aax(AAXSTREAMFILE *streamfile)
{
  return 0;
}

static size_t get_offset_aax(AAXSTREAMFILE *streamfile)
{
  long offset = streamfile->real_file->get_offset(streamfile->real_file);
  offset -= streamfile->start_physical_offset;
  if (offset < 0) offset = 0;
  if (offset > streamfile->file_size) offset = streamfile->file_size;

  return offset;
}

static void get_name_aax(AAXSTREAMFILE *streamfile,char *buffer,size_t length)
{
  strncpy(buffer,"ARBITRARY.ADX",length);
  buffer[length-1]='\0';
}

static STREAMFILE *open_aax_impl(AAXSTREAMFILE *streamfile,const char * const filename,size_t buffersize) 
{
  AAXSTREAMFILE *newfile;
  if (strcmp(filename,"ARBITRARY.ADX"))
      return NULL;

  newfile = malloc(sizeof(AAXSTREAMFILE));
  if (!newfile)
      return NULL;
  memcpy(newfile,streamfile,sizeof(AAXSTREAMFILE));
  return &newfile->sf;
}

static STREAMFILE *open_aax_with_STREAMFILE(STREAMFILE *file,off_t start_offset,size_t file_size)
{
  AAXSTREAMFILE *streamfile = malloc(sizeof(AAXSTREAMFILE));

  if (!streamfile)
    return NULL;
  
  /* success, set our pointers */

  streamfile->sf.read = (void*)read_aax;
  streamfile->sf.get_size = (void*)get_size_aax;
  streamfile->sf.get_offset = (void*)get_offset_aax;
  streamfile->sf.get_name = (void*)get_name_aax;
  streamfile->sf.get_realname = (void*)get_name_aax;
  streamfile->sf.open = (void*)open_aax_impl;
  streamfile->sf.close = (void*)close_aax;
#ifdef PROFILE_STREAMFILE
  streamfile->sf.get_bytes_read = NULL;
  streamfile->sf.get_error_count = NULL;
#endif

  streamfile->real_file = file;
  streamfile->start_physical_offset = start_offset;
  streamfile->file_size = file_size;
  
  return &streamfile->sf;
}

