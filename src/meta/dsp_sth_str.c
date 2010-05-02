#include "meta.h"
#include "../util.h"

/* 
    STH+STR
    found in SpongebobSquarepants: Creature From The Krusty Krab (NGC)
*/

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str1(STREAMFILE *streamFile) {
  	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileSTR = NULL;
    char filename[260];
	  char filenameSTR[260];
  	int i, j;
	  int channel_count;
	  int loop_flag;
    off_t coef_table[8] = {0x12C,0x18C,0x1EC,0x24C,0x2AC,0x30C,0x36C,0x3CC};

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sth",filename_extension(filename))) goto fail;

	  strcpy(filenameSTR,filename);
	  strcpy(filenameSTR+strlen(filenameSTR)-3,"str");
	  streamFileSTR = streamFile->open(streamFile,filenameSTR,STREAMFILE_DEFAULT_BUFFER_SIZE);
	    if (!streamFileSTR) goto fail;

    if (read_32bitBE(0x0,streamFile) != 0x0)
    {
      goto fail;
    }

    if (read_32bitBE(0x4,streamFile) != 0x800)
    {
      goto fail;
    }

    /* Not really channel_count, just 'included tracks * channels per track */
	  loop_flag = (read_32bitBE(0xD8,streamFile) != 0xFFFFFFFF);
    channel_count = (read_32bitBE(0x70,streamFile)) * (read_32bitBE(0x88,streamFile));
    
    if (channel_count > 8)
    {
      goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
      if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = channel_count;
	  vgmstream->sample_rate = read_32bitBE(0x24,streamFile);
	  vgmstream->num_samples=get_streamfile_size(streamFileSTR)/8/channel_count*14;
	  vgmstream->coding_type = coding_NGC_DSP;
	
	  if(loop_flag)
    {
		  vgmstream->loop_start_sample = read_32bitBE(0xD8,streamFile);
		  vgmstream->loop_end_sample = read_32bitBE(0xDC,streamFile);
    }

	if (channel_count == 1)
  {
		vgmstream->layout_type = layout_none;
	}
  else
  {
		vgmstream->layout_type = layout_interleave;
		if (channel_count == 2)
    {
      vgmstream->interleave_block_size=0x10000;
    }
    else
    {
      vgmstream->interleave_block_size=0x8000;
    }
  }

  vgmstream->meta_type = meta_NGC_DSP_STH_STR;

    /* open the file for reading */
    for (i=0;i<channel_count;i++)
    {
      vgmstream->ch[i].streamfile = streamFileSTR->open(streamFileSTR,filenameSTR,0x8000);
        if (!vgmstream->ch[i].streamfile) goto fail;
      vgmstream->ch[i].channel_start_offset=vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
    }

    // COEFFS
    for (j=0;j<vgmstream->channels;j++)
    {
      for (i=0;i<16;i++)
      {
        vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
      }
    }

  	close_streamfile(streamFileSTR); streamFileSTR=NULL;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileSTR) close_streamfile(streamFileSTR);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* 
    STH+STR
    found in Taz Wanted (NGC), Cubix Robots for Everyone: Showdown (NGC)
*/

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str2(STREAMFILE *streamFile) {
  	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileSTR = NULL;
    char filename[260];
	  char filenameSTR[260];
  	int i, j;
	  int channel_count;
	  int loop_flag;
    off_t coef_table[8] = {0xDC,0x13C,0x19C,0x1FC,0x25C,0x2BC,0x31C,0x37C};

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sth",filename_extension(filename))) goto fail;

	  strcpy(filenameSTR,filename);
	  strcpy(filenameSTR+strlen(filenameSTR)-3,"str");
	  streamFileSTR = streamFile->open(streamFile,filenameSTR,STREAMFILE_DEFAULT_BUFFER_SIZE);
	    if (!streamFileSTR) goto fail;

    if (read_32bitBE(0x0,streamFile) != 0x0)
    {
      goto fail;
    }

    if (read_32bitBE(0x4,streamFile) != 0x900)
    {
      goto fail;
    }

    /* Not really channel_count, just 'included tracks * channels per track */
  	loop_flag = (read_32bitBE(0xB8,streamFile) != 0xFFFFFFFF);
    channel_count = read_32bitBE(0x50,streamFile)*2;

    if (channel_count > 8)
    {
      goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
      if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = channel_count;
	  vgmstream->sample_rate = read_32bitBE(0x24,streamFile);
	  vgmstream->num_samples=get_streamfile_size(streamFileSTR)/8/channel_count*14;
	  vgmstream->coding_type = coding_NGC_DSP;
	
	  if(loop_flag)
    {
		  vgmstream->loop_start_sample = read_32bitBE(0xB8,streamFile);
		  vgmstream->loop_end_sample = read_32bitBE(0xBC,streamFile);
    }

	if (channel_count == 1)
  {
		vgmstream->layout_type = layout_none;
	}
  else
  {
		vgmstream->layout_type = layout_interleave;
		if (channel_count == 2)
    {
      vgmstream->interleave_block_size=0x10000;
    }
    else
    {
      vgmstream->interleave_block_size=0x8000;
    }
  }

  vgmstream->meta_type = meta_NGC_DSP_STH_STR;

    /* open the file for reading */
    for (i=0;i<channel_count;i++)
    {
      vgmstream->ch[i].streamfile = streamFileSTR->open(streamFileSTR,filenameSTR,0x8000);
        if (!vgmstream->ch[i].streamfile) goto fail;
      vgmstream->ch[i].channel_start_offset=vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
    }

    // COEFFS
    for (j=0;j<vgmstream->channels;j++)
    {
      for (i=0;i<16;i++)
      {
        vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
      }
    }

  	close_streamfile(streamFileSTR); streamFileSTR=NULL;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileSTR) close_streamfile(streamFileSTR);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* 
    STH+STR
    found in Tak and the Guardians of Gross (WII)
*/

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str3(STREAMFILE *streamFile) {
  	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileSTR = NULL;
    char filename[260];
	  char filenameSTR[260];
  	int i, j;
	  int channel_count;
	  int loop_flag;
    off_t coef_table[8] = {read_32bitBE(0x7C,streamFile),read_32bitBE(0x80,streamFile),read_32bitBE(0x84,streamFile),read_32bitBE(0x88,streamFile),read_32bitBE(0x8C,streamFile),read_32bitBE(0x90,streamFile),read_32bitBE(0x94,streamFile),read_32bitBE(0x98,streamFile)};

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sth",filename_extension(filename))) goto fail;

	  strcpy(filenameSTR,filename);
	  strcpy(filenameSTR+strlen(filenameSTR)-3,"str");
	  streamFileSTR = streamFile->open(streamFile,filenameSTR,STREAMFILE_DEFAULT_BUFFER_SIZE);
	    if (!streamFileSTR) goto fail;

    if (read_32bitBE(0x0,streamFile) != 0x0)
    {
      goto fail;
    }

    if ((read_32bitBE(0x4,streamFile) != 0x700) &&
      (read_32bitBE(0x4,streamFile) != 0x800))
    {
      goto fail;
    }

    /* Not really channel_count, just 'included tracks * channels per track */
  	loop_flag = 0;
    channel_count = read_32bitBE(0x70,streamFile);

    if (channel_count > 8)
    {
      goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
      if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = channel_count;
	  vgmstream->sample_rate = read_32bitBE(0x38,streamFile);
	  vgmstream->num_samples=get_streamfile_size(streamFileSTR)/8/channel_count*14;
	  vgmstream->coding_type = coding_NGC_DSP;
	
	  if(loop_flag)
    {
		  vgmstream->loop_start_sample = 0;
		  vgmstream->loop_end_sample = 0;
    }

	if (channel_count == 1)
  {
		vgmstream->layout_type = layout_none;
	}
  else
  {
		vgmstream->layout_type = layout_interleave;
		if (channel_count == 2 || channel_count == 4)
    {
      vgmstream->interleave_block_size=0x8000;
    }
    else
    {
      vgmstream->interleave_block_size=0x4000;
    }
  }

  vgmstream->meta_type = meta_NGC_DSP_STH_STR;

    /* open the file for reading */
    for (i=0;i<channel_count;i++)
    {
      vgmstream->ch[i].streamfile = streamFileSTR->open(streamFileSTR,filenameSTR,0x8000);
        if (!vgmstream->ch[i].streamfile) goto fail;
      vgmstream->ch[i].channel_start_offset=vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
    }

    // COEFFS
    for (j=0;j<vgmstream->channels;j++)
    {
      for (i=0;i<16;i++)
      {
        vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
      }
    }

  	close_streamfile(streamFileSTR); streamFileSTR=NULL;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileSTR) close_streamfile(streamFileSTR);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
