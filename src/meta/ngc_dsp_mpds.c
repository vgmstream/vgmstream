#include "meta.h"
#include "../util.h"

/* MPDS - found in Big Air Freestyle, Terminator 3 (no coeffs), etc */
VGMSTREAM * init_vgmstream_ngc_dsp_mpds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
  	int channel_count;
    int ch1_start=-1, ch2_start=-1;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("dsp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D504453) /* "MPDS" */
        goto fail;
    /* Version byte ??? */
    if (read_32bitBE(0x04,streamFile) != 0x00010000) /* "0x10000" */
        goto fail;
    /* compare sample count with body size */
    if (((read_32bitBE(0x08,streamFile)/7*8)) != (read_32bitBE(0x0C,streamFile)))
        goto fail;

    channel_count = read_32bitBE(0x14,streamFile);

    if (channel_count > 2)
    {
      goto fail;
    }

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
	  vgmstream->channels = channel_count;

    if (channel_count == 1)
    {
      vgmstream->layout_type = layout_none;
      ch1_start = 0x80;
    }
    else if (channel_count == 2)
    {
      vgmstream->layout_type = layout_interleave;
      vgmstream->interleave_block_size = read_32bitBE(0x18,streamFile);
      ch1_start = 0x80;
      ch2_start = 0x80 + vgmstream->interleave_block_size;
    }
    else
    {
    	goto fail;
    }

    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x08,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitBE(0x08,streamFile);
    }
    
    vgmstream->meta_type = meta_NGC_DSP_MPDS;
    
    {
      int i;
			for (i=0;i<16;i++)
			  vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x24+i*2,streamFile);
			
      if (channel_count == 2)
      {
			  for (i=0;i<16;i++)
			  vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x4C+i*2,streamFile);
      }
    }

    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[0].streamfile) goto fail;
    vgmstream->ch[0].channel_start_offset =
            vgmstream->ch[0].offset=ch1_start;

    if (channel_count == 2)
    {
      vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
      if (!vgmstream->ch[1].streamfile) goto fail;
        vgmstream->ch[1].channel_start_offset = 
        vgmstream->ch[1].offset=ch2_start;
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
