#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_bfwav(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];

	coding_t coding_type;

	off_t head_offset;
	/*off_t seek_offset;*/
	int codec_number;
	int channel_count;
	int loop_flag;

	off_t start_offset;

	int big_endian = 1;
	int ima = 0;
	int nsmbu_flag = 0;
	int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;
	int16_t(*read_16bit)(off_t, STREAMFILE*) = NULL;
	read_16bit = read_16bitBE;
	read_32bit = read_32bitBE;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("bfwav", filename_extension(filename))) {
		if (strcasecmp("bfwavnsmbu",filename_extension(filename))) goto fail;
		else nsmbu_flag = 1;
	}
	
	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x46574156) /* "FWAV" */
		goto fail;

	if ((uint32_t)read_32bitBE(4, streamFile) != 0xFEFF0040) /* "FWAV" */
		goto fail;

	/* get head offset, check */
	if (big_endian)
		head_offset = read_32bit(0x18, streamFile);

	
	if ((uint32_t)read_32bitBE(head_offset, streamFile) != 0x494E464F)  /* "INFO" (FWAV)*/
		goto fail;
	

	/* check type details */
	codec_number = read_8bit(head_offset + 0x8, streamFile);
	loop_flag = read_8bit(head_offset + 0x9, streamFile);
	channel_count = read_8bit(head_offset + 0x1F, streamFile);

	switch (codec_number) {
	case 0:
		coding_type = coding_PCM8;
		break;
	case 1:
		if (big_endian)
			coding_type = coding_PCM16BE;
		else
			coding_type = coding_PCM16LE;
		break;
	case 2:
		coding_type = coding_NGC_DSP;
		break;
	default:
		goto fail;
	}

	if (channel_count < 1) goto fail;

	/* build the VGMSTREAM */

	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->num_samples = read_32bit(head_offset + 0x14, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bit(head_offset + 0xE, streamFile);
	if (nsmbu_flag)
		vgmstream->sample_rate /= 2;
	/* channels and loop flag are set by allocate_vgmstream */

	vgmstream->loop_start_sample = read_32bit(head_offset + 0x10, streamFile);
	vgmstream->loop_end_sample = vgmstream->num_samples;

	vgmstream->coding_type = coding_type;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	else
	{
		if (ima)
			vgmstream->layout_type = layout_interleave;
		else
			vgmstream->layout_type = layout_interleave_shortblock;
	}
	if (big_endian)
		vgmstream->meta_type = meta_FWAV;

	if (big_endian)
	{
	//	vgmstream->interleave_block_size = read_32bit(head_offset + 0x38, streamFile);
	//	vgmstream->interleave_smallblock_size = read_32bit(head_offset + 0x48, streamFile);

		vgmstream->interleave_block_size = 0x200;
		vgmstream->interleave_smallblock_size = 0x20;
	}

	if (big_endian)
		start_offset = read_32bitBE(0x24, streamFile) + 0x20;

	if (vgmstream->coding_type == coding_NGC_DSP) {
		off_t coef_offset;
		/*off_t coef_offset1;
		off_t coef_offset2;*/
		int coef_spacing;
		int i, j;
		off_t coeffheader;
		int foundcoef;

		coef_spacing = 0x2E;

		coeffheader = head_offset + 0x28;
		foundcoef = 0;
		while (!(foundcoef))
		{
			if ((uint32_t)read_32bit(coeffheader, streamFile) == 0x1F000000)
			{
				coef_offset = read_32bit(coeffheader + 0xC, streamFile) + coeffheader;
				foundcoef = 1;
				break;				
			}
			coeffheader++;
		}
			
		

		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				vgmstream->ch[j].adpcm_coef[i] = read_16bit(coef_offset + j*coef_spacing + i * 2, streamFile);
			}
		}
	}



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
