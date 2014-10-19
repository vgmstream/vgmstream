#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_g1l(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];

	coding_t coding_type;

	off_t head_offset;

	int channel_count;
	int loop_flag;
	off_t start_offset;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("g1l", filename_extension(filename)))
		goto fail;


	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x47314C5F) /* "G1L_" */
		goto fail;
	if ((uint32_t)read_32bitBE(0x1c, streamFile) != 0x57696942) /* "WiiB" */
		goto fail;

	/* check type details */
//	loop_flag = read_8bit(head_offset + 0x21, streamFile);
	if (read_32bitBE(0x30, streamFile) > 0)
		loop_flag = 1;
	else
		loop_flag = 0;
	channel_count = read_8bit(0x3f, streamFile);


	coding_type = coding_NGC_DSP;
	

	if (channel_count < 1) goto fail;

	/* build the VGMSTREAM */

	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->num_samples = read_32bitBE(0x2c, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bitBE(0x42, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */
	
	vgmstream->loop_start_sample = read_32bitBE(0x30, streamFile);
	vgmstream->loop_end_sample = vgmstream->num_samples;
	

	vgmstream->coding_type = coding_type;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	
	vgmstream->layout_type = layout_interleave_byte;
	
	vgmstream->meta_type = meta_G1L;

	vgmstream->interleave_block_size = 0x1;	

	if (vgmstream->coding_type == coding_NGC_DSP) {
		off_t coef_offset = 0x78;
		
		int i, j;
		int coef_spacing = 0x60;


		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_offset + j*coef_spacing + i * 2, streamFile);
			}
		}
	}

	if (vgmstream->coding_type == coding_NGC_DSP)
		start_offset = 0x81c;
	else // Will add AT3 G1L support later
		goto fail;



	/* open the file for reading by each channel */
	{
		int i;
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
