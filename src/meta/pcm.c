#include "meta.h"
#include "../util.h"

/* PCM (from Lunar: Eternal Blue (Sega CD) */
VGMSTREAM * init_vgmstream_pcm_scd(STREAMFILE *streamFile) {

	VGMSTREAM * vgmstream = NULL;
	char filename[260];
	off_t start_offset;

    int loop_flag;
    int channel_count;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile,filename,sizeof(filename));
	if (strcasecmp("pcm",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x00020000)
		goto fail;

		loop_flag = (read_32bitLE(0x02,streamFile)!=0);
		channel_count = 1;
	 
		/* build the VGMSTREAM */
		vgmstream = allocate_vgmstream(channel_count,loop_flag);
		if (!vgmstream) goto fail;

		/* fill in the vital statistics */
		start_offset = 0x200;
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = 32000;
		vgmstream->coding_type = coding_PCM8_SB_int;
		vgmstream->num_samples = read_32bitBE(0x06,streamFile)*2;
		if(loop_flag) {
			vgmstream->loop_start_sample = read_32bitBE(0x02,streamFile)*0x400*2;
			vgmstream->loop_end_sample = read_32bitBE(0x06,streamFile)*2;
		}
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x1;
		vgmstream->meta_type = meta_PCM_SCD;

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


/* PCM - Custom header from Konami, which contains only loop infos...
	found in:	Ephemeral Fantasia [Reiselied]
				Yu-Gi-Oh! The Duelists of the Roses [Yu-Gi-Oh! Shin Duel Monsters II]
*/
VGMSTREAM * init_vgmstream_pcm_ps2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("pcm",filename_extension(filename))) goto fail;

	// if ((read_32bitLE(0x00,streamFile)+0x800) != (get_streamfile_size(streamFile)))
	//	goto fail;
	if ((read_32bitLE(0x00,streamFile)) != (read_32bitLE(0x04,streamFile)*4))
		goto fail;

	loop_flag = (read_32bitLE(0x08,streamFile) != 0x0);
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 24000;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = read_32bitLE(0x0,streamFile)/2/channel_count;
    if (loop_flag) {
		vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile);
		vgmstream->loop_end_sample = read_32bitLE(0x0C,streamFile);
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_PCM_PS2;

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
