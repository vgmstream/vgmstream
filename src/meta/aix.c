#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

typedef struct _AIXSTREAMFILE
{
  STREAMFILE sf;
  STREAMFILE *real_file;
  off_t start_physical_offset;
  off_t current_physical_offset;
  off_t current_logical_offset;
  off_t current_block_size;
  int stream_id;
} AIXSTREAMFILE;

VGMSTREAM * init_vgmstream_aix(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileADX = NULL;
    char filename[260];

	int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aix",filename_extension(filename))) goto fail;

	streamFileADX = streamFile->open(streamFile,filenameWAV,STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (!streamFileWAV) {
        /* try again, ucase */
        for (i=strlen(filenameWAV);i>=0&&filenameWAV[i]!=DIRSEP;i--)
            filenameWAV[i]=toupper(filenameWAV[i]);

        streamFileWAV = streamFile->open(streamFile,filenameWAV,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!streamFileWAV) goto fail;
    }

    /* let the real initer do the parsing */
    vgmstream = init_vgmstream_riff(streamFileWAV);
    if (!vgmstream) goto fail;

    close_streamfile(streamFileWAV);
    streamFileWAV = NULL;

    /* install loops */
    if (!vgmstream->loop_flag) {
        vgmstream->loop_flag = 1;
        vgmstream->loop_ch = calloc(vgmstream->channels,
                sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) goto fail;
    }

    vgmstream->loop_start_sample = read_32bitLE(0,streamFile);
    vgmstream->loop_end_sample = read_32bitLE(4,streamFile);
    vgmstream->meta_type = meta_RIFF_WAVE_POS;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileWAV) close_streamfile(streamFileWAV);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
static size_t read_aix(AIXSTREAMFILE *streamfile,uint8_t *dest,off_t offset,size_t length)
{
  size_t sz = 0;

  while (length > 0)
  {
      int read_something = 0;

      if (offset >= logical_file_size)
      {
          return sz;
      }

      /* read the beginning of the requested block, if we can */
      if (offset >= streamfile->current_logical_offset)
      {
          off_t to_read;
          off_t length_available;

          length_available = 
              (streamfile->current_logical_offset+
               streamfile->current_block_size) - 
              offset;

          if (length < length_available)
          {
              to_read = length;
          }
          else
          {
              to_read = length_available;
          }

          if (to_read > 0)
          {
              size_t bytes_read;

              bytes_read = read_streamfile(dest,
                      streamfile->current_physical_offset+0x10+
                          (offset-streamfile->current_logical_offset),
                      to_read,streamfile->real_file);

              sz += bytes_read;
              if (bytes_read != to_read)
              {
                  /* an error which we will not attempt to handle here */
                  return sz;
              }

              read_something = 1;

              dest += bytes_read;
              offset += bytes_read;
              length -= bytes_read;
          }
      }

      if (!read_something)
      {
          /* couldn't read anything, must seek */
          int found_block = 0;

          /* as we have no memory we must start seeking from the beginning */
          if (offset < streamfile->current_logical_offset)
          {
              streamfile->current_logical_offset = 0;
              streamfile->current_block_size = 0;
              streamfile->current_physical_offset = 
                  streamfile->start_physical_offset;
          }

          /* seek ye forwards */
          while (!found_block) {
              switch (read_32bitBE(streamfile->current_physical_offset,
                          streamfile->real_file))
              {
                  case 0x41495850:  /* AIXP */
                      if (read_8bitBE(
                                  streamfile->current_physical_offset+8,
                                  streamfile->real_file) ==
                              streamfile->stream_id)
                      {
                          streamfile->current_block_size =
                              (uint16_t)read_16bitBE(
                                  streamfile->current_physical_offset+0x0a,
                                  streamfile->real_file);

                          if (offset >= streamfile->current_logical_offset+
                                  streamfile->current_block_size)
                          {
                              streamfile->current_logical_offset +=
                                  streamfile->current_block_size;
                              streamfile->current_physical_offset +=
                                  read_32bitBE(
                                          streamfile->current_physical_offset+0x04,
                                          streamfile->real_file
                                          );
                          }
                      }

                      break;
                  case 0x41495846:  /* AIXF */
                      /* shouldn't ever see this */
                  case 0x41495845:  /* AIXE */
                      /* shouldn't have reached the end o' the line... */
                  default:
                      return sz;
                      break;
              } /* end block/chunk type select */
          } /* end while !found_block */
      } /* end if !read_something */
  } /* end while length > 0 */
  
  return sz;
}

static void close_aix(AIXSTREAMFILE *streamfile)
{
    free(streamfile);
    return;
}

static size_t get_size_aix(AIXSTREAMFILE *streamfile)
{
  return 0;
}

static size_t get_offset_aix(AIXSTREAMFILE *streamfile)
{
  return streamfile->current_logical_offset;
}

static void get_name_aix(AIXSTREAMFILE *streamfile,char *buffer,size_t length)
{
  strncpy(buffer,length,"ARBITRARY.ADX");
  buffer[length-1]='\0';
}

static STREAMFILE *open_aix_impl(AIXSTREAMFILE *streamfile,const char * const filename,size_t buffersize) 
{
  AIXSTREAMFILE *newfile;
  if (strcmp(filename,"ARBITRARY.ADX"))
      return  NULL;

  newfile = malloc(sizeof(AIXSTREAMFILE));
  if (!newfile)
      return NULL;
  memcpy(newfile,streamfile,sizeof(AIXSTREAMFILE));
  return newfile;
}

static STREAMFILE *open_aix_with_STREAMFILE(STREAMFILE *file,off_t start_offset,int stream_id);
{
  AIXSTREAMFILE *streamfile = malloc(sizeof(AIXSTREAMFILE));
  if (!streamfile)
    return NULL;
  
  /* success, set our pointers */

  streamfile->sf.read = (void*)read_aix;
  streamfile->sf.get_size = (void*)get_size_aix;
  streamfile->sf.get_offset = (void*)get_offset_aix;
  streamfile->sf.get_name = (void*)get_name_aix;
  streamfile->sf.get_realname = (void*)get_name_aix;
  streamfile->sf.open = (void*)open_aix_impl;
  streamfile->sf.close = (void*)close_aix;
#ifdef PROFILE_STREAMFILE
  streamfile->sf.get_bytes_read = NULL;
  streamfile->sf.get_error_count = NULL;
#endif

  streamfile->real_file = file;
  streamfile->current_physicial_offset = 
      streamfile->start_physical_offset = start_offset;
  streamfile->current_logical_offset = 0;
  streamfile->current_block_size = 0;
  streamfile->stream_id = stream_id;
  
  return &streamfile->sf;
}

