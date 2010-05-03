#include "meta.h"
#include "../util.h"

/*	"idsp/IDSP"
	Soul Calibur Legends (Wii)
	Sky Crawlers: Innocent Aces (Wii)
*/
VGMSTREAM * init_vgmstream_idsp2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
  	int channel_count;
	  int i, j;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("idsp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x69647370 || /* "idsp" */
        read_32bitBE(0xBC,streamFile) != 0x49445350) /* IDSP */
    goto fail;

    loop_flag = read_32bitBE(0x20,streamFile);
    channel_count = read_32bitBE(0xC4,streamFile);
    
    if (channel_count > 8)
    {
      goto fail;
    }

  	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
		start_offset = (channel_count * 0x60) + 0x100;
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = read_32bitBE(0xC8,streamFile);
		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->num_samples = (read_32bitBE(0x14,streamFile))*14/8/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0xD0,streamFile));
        vgmstream->loop_end_sample = (read_32bitBE(0xD4,streamFile));
    }
	    
	  if (channel_count == 1)
    {
			vgmstream->layout_type = layout_none;
    }
    else if (channel_count > 1)
    {
    		if (read_32bitBE(0xD8,streamFile) == 0)
        {
	    	  	vgmstream->layout_type = layout_none;
		    	  vgmstream->interleave_block_size = (get_streamfile_size(streamFile)-start_offset)/2;
        }
        else if (read_32bitBE(0xD8,streamFile) > 0)
        {
			      vgmstream->layout_type = layout_interleave;
			      vgmstream->interleave_block_size = read_32bitBE(0xD8,streamFile);
        }
    }

		vgmstream->meta_type = meta_IDSP;

	  {
		  if (vgmstream->coding_type == coding_NGC_DSP) {
			  off_t coef_table[8] = {0x118,0x178,0x1D8,0x238,0x298,0x2F8,0x358,0x3B8};
			  for (j=0;j<vgmstream->channels;j++) {
				  for (i=0;i<16;i++) {
				  vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
          }
        }
      }
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }


    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* IDSP (Mario Strikers Charged)
    - Single "IDSP" header... */
VGMSTREAM * init_vgmstream_idsp3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 1;
    int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("idsp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* IDSP */
        goto fail;

    channel_count = read_32bitBE(0x24,streamFile);

    if (channel_count > 8)
    {
      goto fail;
    }

  	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = (channel_count*0x60)+0xC;
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x14,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x0C,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitBE(0x0C,streamFile));
    }

    vgmstream->interleave_block_size = read_32bitBE(0x04,streamFile);
		vgmstream->interleave_smallblock_size = ((vgmstream->num_samples/7*8)%(vgmstream->interleave_block_size)/vgmstream->channels);
    vgmstream->layout_type = layout_interleave_shortblock;
		
    vgmstream->meta_type = meta_IDSP;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x28+i*2,streamFile);
        }
        if (vgmstream->channels) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x88+i*2,streamFile);
            }
        }
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* IDSP (Defender NGC) */
VGMSTREAM * init_vgmstream_idsp4(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
  	int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("idsp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;

    channel_count = read_32bitBE(0x0C,streamFile);
    
    if (channel_count > 2) // Refuse everything else for now
    {
      goto fail;
    }

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = 0x70;
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x04,streamFile)/channel_count/8*14;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitBE(0x04,streamFile)/channel_count/8*14;
    }

    if (channel_count == 1)
    {
      vgmstream->layout_type = layout_none;
    }
    else
    {
      vgmstream->layout_type = layout_interleave;
      vgmstream->interleave_block_size = read_32bitBE(0x10,streamFile);
    }
    
    vgmstream->meta_type = meta_IDSP;

			{
				int i;
				for (i=0;i<16;i++)
					vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x14+i*2,streamFile);
				if (channel_count == 2) {
				for (i=0;i<16;i++)
					vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x42+i*2,streamFile);
				}
      }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
