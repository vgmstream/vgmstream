#include "meta.h"
#include "../coding/coding.h"

/* Ongakukan RIFF with "ADP" extension [Train Simulator - Midousuji-sen (PS2)] */
VGMSTREAM* init_vgmstream_ongakukan_adp(STREAMFILE* sf)
{
	VGMSTREAM* vgmstream = NULL;
	off_t start_offset;
	size_t file_size;
	int has_data_chunk = 0, has_fact_chunk = 0, found_start_offset = 0;
	int loop_flag = 0;
	int riff_wave_header_size = 0x2c;
	char sound_is_adpcm = 0, sample_has_base_setup_from_the_start = 0;
	/* ^ the entire RIFF WAVE header size, set to this fixed number
	 * because *surprise* this is also how sound data begins. */
	int32_t fmt_size, fmt_offset, offset_of_supposed_last_chunk;
	int32_t sample_rate, bitrate, data_size;
	int16_t num_channels, block_size;

	/* RIFF+WAVE checks */
	if (!is_id32be(0x00, sf, "RIFF")) goto fail;
	if (!is_id32be(0x08, sf, "WAVE")) goto fail;
	/* WAVE "fmt " check */
	if (!is_id32be(0x0c, sf, "fmt ")) goto fail;
	/* "adp" extension check (literally only one) */
	if (!check_extensions(sf, "adp")) goto fail;
	
	/* catch adp file size from here and use it whenever needed. */
	file_size = get_streamfile_size(sf);

	/* RIFF size from adp file (e.g: 10MB) can go beyond actual adp file size (e.g: 2MB),
	 * have vgmstream call it quits if the former is reported to be less than the latter. */
	if (read_s32le(0x04, sf) < file_size) goto fail;

	/* read entire WAVE "fmt " chunk. we start by reading fmt_size from yours truly and setting fmt_offset. */
	fmt_size = read_s32le(0x10, sf);
	fmt_offset = 0x14;
	if ((fmt_size > 0x10) && (fmt_size < 0x13)) /* fmt_size is mostly 0x10, rarely 0x12 */
	{
		if (read_s16le(fmt_offset + 0, sf) != 1) goto fail; /* chunk reports codec number as signed little-endian PCM, couldn't be more wrong. */
		num_channels = read_s16le(fmt_offset + 2, sf);
		sample_rate = read_s32le(fmt_offset + 4, sf);
		bitrate = read_s32le(fmt_offset + 8, sf);
		/* ^ yes, this is technically correct, tho does not reflect actual data.
		 * chunk reports bitrate as if it was a 16-bit PCM file. */
		block_size = read_s16le(fmt_offset + 12, sf); /* mostly 2, rarely 4. */
		if (read_s16le(fmt_offset + 14, sf) != 0x10) goto fail; /* bit depth as chunk reports it. */
		/* additional checks, this time with bitrate field in the chunk. */
		if (bitrate != (sample_rate * block_size)) goto fail;
		/* if fmt_size == 0x12 there is an additional s16 field that goes unused. */
	}
	else {
		goto fail;
	}

	/* now calc the var so we can read either "data" or "fact" chunk; */
	offset_of_supposed_last_chunk = fmt_offset + fmt_size;

	/* then read either one of the two chunks, both cannot co-exist it seems.
	 * while there, we set start_offset by themselves instead of relying on both chunks to do so.
	 * see comments for code handling both chunks below for more info. */
	if (is_id32be(offset_of_supposed_last_chunk + 0, sf, "data")) has_data_chunk = 1;
	if (is_id32be(offset_of_supposed_last_chunk + 0, sf, "fact")) has_fact_chunk = 1;

	/* and because sound data *must* start at 0x2c, they have to bork both chunks too, so they're now essentially useless. 
	 * well, except for trying to deduct how many samples a sound actually has*/
	if (has_data_chunk)
	{
		/* RIFF adp files have borked "data" chunk so much it's not even remotely useful for... well, *anything*, really.
		 * it doesn't report actual data size as RIFF WAVE files usually do, instead we're left with basically RIFF size with ~50 less numbers now. */
		if (read_s32le(offset_of_supposed_last_chunk + 4, sf) < file_size) goto fail;
		/* ^ reported data size is meant to be bigger than actual adp size, have vgmstream throw out the towel if it isn't. */
	}

	if (has_fact_chunk)
	{
		/* RIFF adp files also borked "fact" chunk so it no longer reports useful info.
		 * instead it just leaves out a s16 field containing a static number. */
		if (read_s16le(offset_of_supposed_last_chunk + 4, sf) != 4) goto fail;
		/* ^ this number is supposed to be 4, have vgmstream ragequit if it isn't. */
	}

	/* set start_offset value to riff_wave_header_size
	 * and calculate data_size by ourselves */
	start_offset = riff_wave_header_size;
	data_size = (int32_t)(file_size) - riff_wave_header_size;

	/* Ongagukan games using this format just read it by checking "ADP" extension
	 * in an provided file name of a programmer's own choosing,
	 * and if it's there they just read the reported "number of samples" and sample_rate from RIFF WAVE "fmt " chunk 
	 * based on an already-opened file with that same name.
	 * they also calculate start_offset and data_size in much the same manner. */

	 /* silly flags, needed to init our custom decoder. */
	sound_is_adpcm = 1;
	sample_has_base_setup_from_the_start = 1;

	/* build the VGMSTREAM */
	vgmstream = allocate_vgmstream(num_channels, loop_flag);
	if (!vgmstream) goto fail;

	vgmstream->meta_type = meta_ONGAKUKAN_RIFF_ADP;
	vgmstream->sample_rate = sample_rate;
	vgmstream->codec_data = init_ongakukan_adp(sf, start_offset, data_size, sound_is_adpcm, sample_has_base_setup_from_the_start);
	if (!vgmstream->codec_data) goto fail;
	vgmstream->coding_type = coding_ONGAKUKAN_ADPCM;
	vgmstream->layout_type = layout_none;
	vgmstream->num_samples = ongakukan_adp_get_samples(vgmstream->codec_data);

	if (!vgmstream_open_stream(vgmstream, sf, start_offset))
		goto fail;
	return vgmstream;
fail:
	close_vgmstream(vgmstream);
	return NULL;
}
