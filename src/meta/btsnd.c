/*
Wii U boot sound file for each game/app.
*/

#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_btsnd(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	int channel_count = 2;
	int loop_flag;
	off_t start_offset = 0x8;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("btsnd", filename_extension(filename))) 
		goto fail;
	
	/* Checking for loop start */
	if (read_32bitBE(0x4, streamFile) > 0)
		loop_flag = 1;
	else
		loop_flag = 0;
		
	if (channel_count < 1) goto fail;

	/* build the VGMSTREAM */

	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->sample_rate = 48000;
	/* channels and loop flag are set by allocate_vgmstream */
	
	// There's probably a better way to get the sample count...
	vgmstream->num_samples = vgmstream->loop_end_sample = (get_streamfile_size(streamFile) - 8) / 4;
	
	vgmstream->loop_start_sample = read_32bitBE(0x4, streamFile);
		
	vgmstream->coding_type = coding_PCM16BE;
	vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = 0x2;	// Constant for this format
	vgmstream->meta_type = meta_WIIU_BTSND;
	
	/* open the file for reading by each channel */
	{
		int i;
		for (i = 0; i<channel_count; i++) {
				vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename,
				STREAMFILE_DEFAULT_BUFFER_SIZE);

			if (!vgmstream->ch[i].streamfile) goto fail;

			vgmstream->ch[i].channel_start_offset =
				vgmstream->ch[i].offset =
				start_offset + i*vgmstream->interleave_block_size;
		}
	}

	return vgmstream;

	/* clean up anything we may have opened */
fail:
	if (vgmstream) close_vgmstream(vgmstream);
	return NULL;
}
