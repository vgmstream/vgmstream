#include "meta.h"
#include "../util.h"

/* BO2 (Blood Omen 2 NGC) */
VGMSTREAM * init_vgmstream_ngc_bo2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
  	int channels;
    int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bo2",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x0) /* "IDSP" */
        goto fail;

    switch (read_32bitBE(0x10,streamFile))
    {
    	case 0x0:
    	  channels = 1;
    	break;
      case 0x1:
        channels = 2;
      break;
        default:
          goto fail;
    }
    
    if ((get_streamfile_size(streamFile)) < ((read_32bitBE(0x0C,streamFile)/14*8*channels)+0x800))
    {
      goto fail;
    }

    channel_count = channels;
    loop_flag = (read_32bitBE(0x08,streamFile) != 0xFFFFFFFF);

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = 0x800;
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x04,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x0C,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x08,streamFile);
        vgmstream->loop_end_sample = read_32bitBE(0x0C,streamFile);
    }

    if (channel_count == 1)
    {
      vgmstream->layout_type = layout_none;
    }
    else
    {
      vgmstream->layout_type = layout_interleave;
      vgmstream->interleave_block_size = 0x400;
    }
    
    vgmstream->meta_type = meta_NGC_BO2;

			{
				int i;
				for (i=0;i<16;i++)
					vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x24+i*2,streamFile);
				if (channel_count == 2) {
				for (i=0;i<16;i++)
					vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x52+i*2,streamFile);
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
