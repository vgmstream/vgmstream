#include "meta.h"
#include "../util.h"

/* STM: Red Dead Revolver */
VGMSTREAM * init_vgmstream_ps2_stm(STREAMFILE *streamFile) {

	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	off_t start_offset;

    int loop_flag;
    int channel_count;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile,filename,sizeof(filename));
	if (strcasecmp("ps2stm",filename_extension(filename))) goto fail;

	/* check header */
    if (read_32bitBE(0x0,streamFile) != 0x53544d41) goto fail;
    if (read_32bitBE(0x4,streamFile) != 0x6b690000) goto fail;

    /* check bps */
    if (read_32bitLE(0x10,streamFile) != 4) goto fail;

    loop_flag = read_32bitLE(0x20,streamFile);
    channel_count = read_32bitLE(0x14,streamFile);
	 
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x800;
    vgmstream->sample_rate = (uint16_t)read_32bitLE(0xc,streamFile);

    vgmstream->coding_type = coding_DVI_IMA_int;

    vgmstream->num_samples = read_32bitLE(0x18,streamFile);

    //vgmstream->interleave_block_size = read_32bitLE(0x8,streamFile) / channel_count;
    vgmstream->interleave_block_size = 0x40;

	if(1 < channel_count)
	{
		/* not sure if this is right ... */
		vgmstream->layout_type = layout_interleave;
	} else {
		vgmstream->layout_type = layout_none;
	}
    vgmstream->meta_type = meta_PS2_STM;

	if(loop_flag) {
		vgmstream->loop_start_sample=read_32bitLE(0x24,streamFile);
		vgmstream->loop_end_sample=vgmstream->num_samples;
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
