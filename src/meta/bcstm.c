#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_bcstm(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];

	coding_t coding_type;

	off_t head_offset;
	int codec_number;
	int channel_count;
	int loop_flag;
	off_t start_offset;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("bcstm", filename_extension(filename))) 
		goto fail;


	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x4353544D) /* "CSTM" */
		goto fail;
	if ((uint32_t)read_32bitBE(4, streamFile) != 0xFFFE4000)
		goto fail;
	
	

	/* get head offset, check */
	head_offset = read_32bitLE(0x18, streamFile);

	if ((uint32_t)read_32bitBE(head_offset, streamFile) != 0x494E464F) /* "INFO" */
		goto fail;
	

	/* check type details */
	codec_number = read_8bit(head_offset + 0x20, streamFile);
	loop_flag = read_8bit(head_offset + 0x21, streamFile);
	channel_count = read_8bit(head_offset + 0x22, streamFile);

	switch (codec_number) {
	case 0:
		coding_type = coding_PCM8;
		break;
	case 1:
		coding_type = coding_PCM16BE;
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
	vgmstream->num_samples = read_32bitLE(head_offset + 0x2c, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bitLE(head_offset + 0x24, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */
	vgmstream->loop_start_sample = read_32bitLE(head_offset + 0x28, streamFile);
	vgmstream->loop_end_sample = vgmstream->num_samples;

	vgmstream->coding_type = coding_type;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	else
		vgmstream->layout_type = layout_interleave_shortblock;
	vgmstream->meta_type = meta_CSTM;

	vgmstream->interleave_block_size = read_32bitLE(head_offset + 0x34, streamFile);
	vgmstream->interleave_smallblock_size = read_32bitLE(head_offset + 0x44, streamFile);

	if (vgmstream->coding_type == coding_NGC_DSP) {
		off_t coef_offset;
		off_t coef_offset1;
		off_t coef_offset2;
		int i, j;
		int coef_spacing = 0x2E;

		
		coef_offset1 = read_32bitLE(head_offset + 0x1c, streamFile);
		coef_offset = coef_offset1 + 0x40;
	

		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				vgmstream->ch[j].adpcm_coef[i] = read_16bitLE(head_offset + coef_offset + j*coef_spacing + i * 2, streamFile);
			}
		}
	}

	start_offset = read_32bitLE(0x30, streamFile) + 0x20;

	/* open the file for reading by each channel */
	{
		int i;
		for (i = 0; i<channel_count; i++) {
			if (vgmstream->layout_type == layout_interleave_shortblock)
				vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename,
				vgmstream->interleave_block_size);
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
