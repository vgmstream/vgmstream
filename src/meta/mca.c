#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* Capcom MADP - found in Capcom 3DS games */
VGMSTREAM * init_vgmstream_mca(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	int channel_count, loop_flag, version;
	size_t head_size, data_size, file_size;
	off_t start_offset, coef_offset, coef_start, coef_shift;
	int coef_spacing;

	/* check extension, case insensitive */
	if (!check_extensions(streamFile,"mca"))
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

	if (vgmstream->loop_end_sample > vgmstream->num_samples) /* some MH3U songs, somehow */
	    vgmstream->loop_end_sample = vgmstream->num_samples;

	vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
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

	    start_offset = get_streamfile_size(streamFile) - data_size; /* usually head_size but not for some MH3U songs */
	    coef_offset = coef_start + coef_shift * 0x14;

	} else { /* v5: Ace Attourney 6, Monster Hunter Generations, v6+? */
        head_size = read_16bitLE(0x1c, streamFile); /* partial size */
        coef_shift = read_16bitLE(0x28, streamFile);
        coef_start = head_size - coef_spacing * channel_count;

        start_offset = read_32bitLE(coef_start - 0x4, streamFile);
        coef_offset = coef_start + coef_shift * 0x14;
	}

	/* sanity check (for bad rips with the header manually truncated to in attempt to "fix" v5 headers) */
	file_size = get_streamfile_size(streamFile);

	if (start_offset + data_size > file_size) {
		if (head_size + data_size > file_size)
			goto fail;

		start_offset = file_size - data_size;
	}

    /* set up ADPCM coefs  */
	dsp_read_coefs_le(vgmstream, streamFile, coef_offset, coef_spacing);

    /* open the file for reading */
	if ( !vgmstream_open_stream(vgmstream,streamFile, start_offset) )
	    goto fail;

	return vgmstream;

fail:
	close_vgmstream(vgmstream);
	return NULL;
}
