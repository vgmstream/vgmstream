#include "meta.h"
#include "../util.h"

/* SPT+SPT

   2008-11-27 - manakoAT : First try for splitted files...
*/

VGMSTREAM * init_vgmstream_spt_spd(STREAMFILE *streamFile) {
	  
  VGMSTREAM * vgmstream = NULL;
  STREAMFILE * streamFileSPT = NULL;
  char filename[PATH_LIMIT];
	char filenameSPT[PATH_LIMIT];
	int channel_count;
	int loop_flag;
  int i;

  /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("spd",filename_extension(filename))) goto fail;

  	strcpy(filenameSPT,filename);
	  strcpy(filenameSPT+strlen(filenameSPT)-3,"spt");

	  streamFileSPT = streamFile->open(streamFile,filenameSPT,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!streamFileSPT)
        goto fail;
    
    if (read_32bitBE(0x0,streamFileSPT) != 0x1) // make sure that it's not a container
        goto fail;

	  channel_count = 1;
	  loop_flag = (read_32bitBE(0x0C,streamFileSPT) == 0x2);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = channel_count;
	  vgmstream->sample_rate = read_32bitBE(0x08,streamFileSPT);

	  switch ((read_32bitBE(0x4,streamFileSPT))) {
		  case 0:
      case 1:
			  vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->num_samples=read_32bitBE(0x14,streamFileSPT)*14/16/channel_count;
  	    if(loop_flag) {
	  	  vgmstream->loop_start_sample = 0;
		    vgmstream->loop_end_sample = read_32bitBE(0x14,streamFileSPT)*14/16/channel_count;
        }
      break;
		  case 2:
		    vgmstream->coding_type = coding_PCM16BE;
        vgmstream->num_samples=read_32bitBE(0x14,streamFileSPT)/channel_count;
       	if(loop_flag) {
  	  	vgmstream->loop_start_sample = 0;
	  	  vgmstream->loop_end_sample = read_32bitBE(0x14,streamFileSPT)/channel_count;
        }
			break;
		    default:
	        goto fail;
    }

  	if (channel_count == 1) {
	  	  vgmstream->layout_type = layout_none;
	  } else if (channel_count == 2) {
		    vgmstream->layout_type = layout_interleave;
		    vgmstream->interleave_block_size=(read_32bitBE(0x34,streamFileSPT)*channel_count)/2;
    }

    vgmstream->meta_type = meta_SPT_SPD;
    vgmstream->allow_dual_stereo = 1;

    /* open the file for reading */
    {
        for (i=0;i<channel_count;i++) {
			/* Not sure, i'll put a fake value here for now */
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            vgmstream->ch[i].offset = 0;
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }


    
	if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x20+i*2,streamFileSPT);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x40+i*2,streamFileSPT);
            }
        }
    }


	close_streamfile(streamFileSPT); streamFileSPT=NULL;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileSPT) close_streamfile(streamFileSPT);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
