#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_bfwav(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];

	coding_t coding_type;
	coding_t coding_PCM16;
	int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
	int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

	/*int ima = 0;*/
	int nsmbu_flag = 0;

	off_t data_offset;
	off_t head_offset;
	int codec_number;
	int channel_count;
	int loop_flag;

	off_t start_offset;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("bfwav", filename_extension(filename)) && strcasecmp("fwav", filename_extension(filename))) {
		if (strcasecmp("bfwavnsmbu",filename_extension(filename))) goto fail;
		else nsmbu_flag = 1;
	}
	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x46574156) /* "FWAV" */
		goto fail;

	if ((uint16_t)read_16bitBE(4, streamFile) == 0xFEFF) { /* endian marker (BE most common) */
		read_32bit = read_32bitBE;
		read_16bit = read_16bitBE;
		coding_PCM16 = coding_PCM16BE;
	} else if ((uint16_t)read_16bitBE(4, streamFile) == 0xFFFE) { /* LE endian marker */
		read_32bit = read_32bitLE;
		read_16bit = read_16bitLE;
		coding_PCM16 = coding_PCM16LE;
	} else {
		goto fail;
	}

	/* get head offset, check */
	head_offset = read_32bit(0x18, streamFile);		
	data_offset = read_32bit(0x24, streamFile);
	
	if ((uint32_t)read_32bitBE(head_offset, streamFile) != 0x494E464F)  /* "INFO" (FWAV)*/
		goto fail;
	

	/* check type details */
	codec_number = read_8bit(head_offset + 0x8, streamFile);
	loop_flag = read_8bit(head_offset + 0x9, streamFile);
	channel_count = read_32bit(head_offset + 0x1C, streamFile);

	switch (codec_number) {
	case 0:
		coding_type = coding_PCM8;
		break;
	case 1:
		coding_type = coding_PCM16;
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
	if (nsmbu_flag)
		vgmstream->sample_rate = 16000;
	else
		vgmstream->sample_rate = (uint16_t)read_32bit(head_offset + 0xC, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */

	vgmstream->loop_start_sample = read_32bit(head_offset + 0x10, streamFile);
	vgmstream->loop_end_sample = vgmstream->num_samples;

	vgmstream->coding_type = coding_type;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	else
		vgmstream->layout_type = layout_interleave;


	vgmstream->meta_type = meta_FWAV;

	vgmstream->interleave_block_size = read_32bit(read_32bit(0x6c, streamFile) + 0x60, streamFile) - 0x18;
	

	start_offset = data_offset + 0x20;

	if (vgmstream->coding_type == coding_NGC_DSP) {
		int i, j;

		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				off_t coeffheader = head_offset + 0x1C + read_32bit(head_offset + 0x24 + (j*8), streamFile);
				off_t coef_offset;
				if ((uint32_t)read_16bit(coeffheader, streamFile) != 0x1F00) goto fail;

				coef_offset = read_32bit(coeffheader + 0xC, streamFile) + coeffheader;
				vgmstream->ch[j].adpcm_coef[i] = read_16bit(coef_offset + i * 2, streamFile);
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
