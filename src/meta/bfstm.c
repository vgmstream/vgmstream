#include "meta.h"
#include "../util.h"
#include "../stack_alloc.h"

/* BFSTM - Nintendo Wii U format */
VGMSTREAM * init_vgmstream_bfstm(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	coding_t coding_type;
	coding_t coding_PCM16;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

	off_t info_offset = 0, seek_offset = 0, data_offset = 0;
	uint16_t temp_id;
	int codec_number;
	int channel_count, loop_flag;
	int i, j;
	int ima = 0;
	off_t start_offset;
	off_t tempoffset1;
	int section_count;

	/* check extension, case insensitive */
	if ( !check_extensions(streamFile,"bfstm") )
		goto fail;

	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x4653544D) /* "FSTM" */
		goto fail;

	if ((uint16_t)read_16bitBE(4, streamFile) == 0xFEFF) { /* endian marker (BE most common) */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        coding_PCM16 = coding_PCM16BE;
	} else if ((uint16_t)read_16bitBE(4, streamFile) == 0xFFFE) { /* Blaster Master Zero 3DS */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        coding_PCM16 = coding_PCM16LE;
	} else {
	    goto fail;
	}

	section_count = read_16bit(0x10, streamFile);
	for (i = 0; i < section_count; i++) {
		temp_id = read_16bit(0x14 + i * 0xc, streamFile);
		switch(temp_id) {
			case 0x4000:
				info_offset = read_32bit(0x18 + i * 0xc, streamFile);
				/* size_t info_size = read_32bit(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4001:
				seek_offset = read_32bit(0x18 + i * 0xc, streamFile);
				/* size_t seek_size = read_32bit(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4002:
				data_offset = read_32bit(0x18 + i * 0xc, streamFile);
				/* size_t data_size = read_32bit(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4003:
			    /* off_t regn_offset = read_32bit(0x18 + i * 0xc, streamFile); */
				/* size_t regn_size = read_32bit(0x1c + i * 0xc, streamFile); */
				break;
			case 0x4004:
				/* off_t pdat_offset = read_32bit(0x18 + i * 0xc, streamFile); */
				/* size_t pdat_size = read_32bit(0x1c + i * 0xc, streamFile); */
				break;
			default:
				break;				
		}
	}
	
    if (info_offset == 0) goto fail;
	if ((uint32_t)read_32bitBE(info_offset, streamFile) != 0x494E464F) /* "INFO" */
		goto fail;


	/* check type details */
	codec_number = read_8bit(info_offset + 0x20, streamFile);
	loop_flag = read_8bit(info_offset + 0x21, streamFile);
	channel_count = read_8bit(info_offset + 0x22, streamFile);

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
	vgmstream->num_samples = read_32bit(info_offset + 0x2c, streamFile);
	vgmstream->sample_rate = read_32bit(info_offset + 0x24, streamFile);
	/* channels and loop flag are set by allocate_vgmstream */
	if (ima) //Shift the loop points back slightly to avoid stupid pops in some IMA streams due to DC offsetting
	{
		vgmstream->loop_start_sample = read_32bit(info_offset + 0x28, streamFile);
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
		vgmstream->loop_start_sample = read_32bit(info_offset + 0x28, streamFile);
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}

	vgmstream->coding_type = coding_type;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
	vgmstream->meta_type = meta_FSTM;

	if (ima)
		vgmstream->interleave_block_size = 0x200;
	else {
		vgmstream->interleave_block_size = read_32bit(info_offset + 0x34, streamFile);
		vgmstream->interleave_last_block_size = read_32bit(info_offset + 0x44, streamFile);
	}

	if (vgmstream->coding_type == coding_NGC_DSP) {
		off_t coeff_ptr_table;
		VARDECL(off_t, coef_offset);
		ALLOC(coef_offset, channel_count, off_t);
		coeff_ptr_table = read_32bit(info_offset + 0x1c, streamFile) + info_offset + 8;	// Getting pointer for coefficient pointer table
		
		for (i = 0; i < channel_count; i++) {
			tempoffset1 = read_32bit(coeff_ptr_table + 8 + i * 8, streamFile);
			coef_offset[i] = tempoffset1 + coeff_ptr_table;
			coef_offset[i] += read_32bit(coef_offset[i] + 4, streamFile);
		} 
		
		for (j = 0; j<vgmstream->channels; j++) {
			for (i = 0; i<16; i++) {
				vgmstream->ch[j].adpcm_coef[i] = read_16bit(coef_offset[j] + i * 2, streamFile);
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
	if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
	    goto fail;

	return vgmstream;

fail:
	close_vgmstream(vgmstream);
	return NULL;
}
