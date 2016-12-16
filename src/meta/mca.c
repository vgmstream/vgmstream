/*
Capcom MADP format found in Capcom 3DS games.
*/

#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_mca(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	int channel_count;
	int loop_flag;
	int version;
	size_t head_size, data_size;
	off_t start_offset, coef_offset, coef_start, coef_shift;
	int i, j;
	int coef_spacing;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("mca", filename_extension(filename))) 
		goto fail;


	/* check header */
	if ((uint32_t)read_32bitBE(0, streamFile) != 0x4D414450) /* "MADP" */
		goto fail;
	
	channel_count = read_8bit(0x8, streamFile);
    if (channel_count < 1) goto fail;
    loop_flag = read_32bitLE(0x18, streamFile) > 0;

	/* build the VGMSTREAM */
	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

    vgmstream->interleave_block_size = read_16bitLE(0xa, streamFile); /* guessed, only seen 0x100 */
	vgmstream->num_samples = read_32bitLE(0xc, streamFile);
	vgmstream->sample_rate = (uint16_t)read_16bitLE(0x10, streamFile);
	vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile);
	vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile);

	vgmstream->coding_type = coding_NGC_DSP;
	if (channel_count == 1)
		vgmstream->layout_type = layout_none;
	else
		vgmstream->layout_type = layout_interleave;
	vgmstream->meta_type = meta_MCA;
	

    /* find data/coef offsets (guessed, formula may change with version) */
	version = read_16bitLE(0x04, streamFile);
    coef_spacing = 0x30;
	data_size = read_32bitLE(0x20, streamFile);

	if (version <= 0x3) { /* v3: Resident Evil Mercenaries 3D, Super Street Fighter IV 3D */
	    head_size = get_streamfile_size(streamFile) - data_size; /* probably 0x2c + 0x30*ch */
        coef_shift = 0x0;
        coef_start = head_size - coef_spacing * channel_count;

	    start_offset = head_size;
	    coef_offset = coef_start + coef_shift * 0x14;

	} else if (version == 0x4) { /* v4: EX Troopers, Ace Attourney 5 */
	    head_size = read_16bitLE(0x1c, streamFile);
        coef_shift = read_16bitLE(0x28, streamFile);
        coef_start = head_size - coef_spacing * channel_count;

	    start_offset = head_size;
	    coef_offset = coef_start + coef_shift * 0x14;

	} else { /* v5: Ace Attourney 6, Monster Hunter Generations, v6+? */
        head_size = read_16bitLE(0x1c, streamFile); /* partial size */
        coef_shift = read_16bitLE(0x28, streamFile);
        coef_start = head_size - coef_spacing * channel_count;

        start_offset = read_32bitLE(coef_start - 0x4, streamFile);
        coef_offset = coef_start + coef_shift * 0x14;
	}

	
	/* set up ADPCM coefs  */
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
