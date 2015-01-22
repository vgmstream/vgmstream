/*
Capcom MADP format found in Capcom 3DS games.
*/

#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_mca(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	coding_t coding_type;
	int channel_count;
	int loop_flag;
	off_t start_offset;
	off_t coef_offset;
	int i, j;
	int coef_spacing;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("mca", filename_extension(filename))) 
		goto fail;


	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x4D414450) /* "MADP" */
		goto fail;
	
	start_offset = (get_streamfile_size(streamFile) - read_32bitLE(0x20, streamFile));
	
	channel_count = read_8bit(0x8, streamFile);

	if (read_32bitLE(0x18, streamFile) > 0)
		loop_flag = 1;
	else
		loop_flag = 0;
	coding_type = coding_NGC_DSP;
	
	if (channel_count < 1) goto fail;

	/* build the VGMSTREAM */

	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->num_samples = read_32bitLE(0xc, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bitLE(0x10, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */

	vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile);
	vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile);

	vgmstream->coding_type = coding_type;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	else
		vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = 0x100;	// Constant for this format
	vgmstream->meta_type = meta_MCA;
	
	
	
	coef_offset = start_offset - (vgmstream->channels * 0x30);
	coef_spacing = 0x30;
	
	for (j = 0; j<vgmstream->channels; j++) {
		for (i = 0; i<16; i++) {
			vgmstream->ch[j].adpcm_coef[i] = read_16bitLE(coef_offset + j*coef_spacing + i * 2, streamFile);
		}
	}
		

	/* open the file for reading by each channel */
	{
		for (i = 0; i<channel_count; i++) {
			if (vgmstream->layout_type == layout_interleave_shortblock)
				vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename,
				vgmstream->interleave_block_size);
			else if (vgmstream->layout_type == layout_interleave)
				vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename,
				STREAMFILE_DEFAULT_BUFFER_SIZE);
			else
				vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename,
				0x1000);

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
