#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_bcstm(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];

	coding_t coding_type;

	off_t info_offset = 0, seek_offset = 0, data_offset = 0;
	uint16_t temp_id;
	int codec_number;
	int channel_count;
	int loop_flag;
	int i, ima = 0;
	off_t start_offset;
	int section_count;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("bcstm", filename_extension(filename))) 
		goto fail;


	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x4353544D) /* "CSTM" */
		goto fail;
	if ((uint16_t)read_16bitLE(4, streamFile) != 0xFEFF)
		goto fail;
	
	section_count = read_16bitLE(0x10, streamFile);
	for (i = 0; i < section_count; i++) {
		temp_id = read_16bitLE(0x14 + i * 0xc, streamFile);
		switch(temp_id) {
			case 0x4000:
				info_offset = read_32bitLE(0x18 + i * 0xc, streamFile);
				/* size_t info_size = read_32bitLE(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4001:
				seek_offset = read_32bitLE(0x18 + i * 0xc, streamFile);
				/* size_t seek_size = read_32bitLE(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4002:
				data_offset = read_32bitLE(0x18 + i * 0xc, streamFile);
				/* size_t data_size = read_32bitLE(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4003:
				/* off_t regn_offset = read_32bitLE(0x18 + i * 0xc, streamFile); */
				/* size_t regn_size = read_32bitLE(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4004:
			    /* off_t pdat_offset = read_32bitLE(0x18 + i * 0xc, streamFile); */
				/* size_t pdat_size = read_32bitLE(0x1c + i * 0xc, streamFile); */
				break;
			default:
				break;				
		}
	}
	
	
	
	
	/* check type details */
	if (info_offset == 0) goto fail;
	codec_number = read_8bit(info_offset + 0x20, streamFile);
	loop_flag = read_8bit(info_offset + 0x21, streamFile);
	channel_count = read_8bit(info_offset + 0x22, streamFile);

	switch (codec_number) {
	case 0:
		coding_type = coding_PCM8;
		break;
	case 1:
		coding_type = coding_PCM16LE;
		break;
	case 2:
	    if (seek_offset == 0) goto fail;
		if ((uint32_t)read_32bitBE(seek_offset, streamFile) != 0x5345454B) { /* "SEEK" If this header doesn't exist, assuming that the file is IMA */
			ima = 1;
			coding_type = coding_INT_IMA;	
		}
		else
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
	vgmstream->num_samples = read_32bitLE(info_offset + 0x2c, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bitLE(info_offset + 0x24, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */
	if (ima) //Shift the loop points back slightly to avoid stupid pops in some IMA streams due to DC offsetting
	{
		vgmstream->loop_start_sample = read_32bitLE(info_offset + 0x28, streamFile);
		if (vgmstream->loop_start_sample > 10000)
		{
			vgmstream->loop_start_sample -= 5000;
			vgmstream->loop_end_sample = vgmstream->num_samples - 5000;
		}
		else
			vgmstream->loop_end_sample = vgmstream->num_samples;
	}
	else
	{
		vgmstream->loop_start_sample = read_32bitLE(info_offset + 0x28, streamFile);
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}

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
	vgmstream->meta_type = meta_CSTM;
	
	if (ima)
		vgmstream->interleave_block_size = 0x200;
	else {
		vgmstream->interleave_block_size = read_32bitLE(info_offset + 0x34, streamFile);
		vgmstream->interleave_smallblock_size = read_32bitLE(info_offset + 0x44, streamFile);
	}
	
	if (vgmstream->coding_type == coding_NGC_DSP) {
		off_t coef_offset;
		off_t tempoffset = info_offset;
		int foundcoef = 0;
		int i, j;
		int coef_spacing = 0x2E;
		
		while (!(foundcoef))
		{
			if ((uint32_t)read_32bitLE(tempoffset, streamFile) == 0x00004102)
			{
				coef_offset = read_32bitLE(tempoffset + 4, streamFile) + tempoffset + (channel_count * 8) - 4 - info_offset;
				foundcoef++;
				break;
			}
			tempoffset++;
		}

		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				vgmstream->ch[j].adpcm_coef[i] = read_16bitLE(info_offset + coef_offset + j*coef_spacing + i * 2, streamFile);
			}
		}
	}
	
	if (ima) { // No SEEK (ADPC) header, so just start where the SEEK header is supposed to be.
	    if (seek_offset == 0) goto fail;
		start_offset = seek_offset;
	} else {
	    if (data_offset == 0) goto fail;
		start_offset = data_offset + 0x20;
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
