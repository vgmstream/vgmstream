#include "meta.h"
#include "../coding/coding.h"

/* AAC - Tri-Ace Audio Container */

/* Xbox 360 Variants (Star Ocean 4, End of Eternity, Infinite Undiscovery) */
VGMSTREAM * init_vgmstream_ta_aac_x360(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
	size_t sampleRate, numSamples, startSample, dataSize, blockSize, blockCount; // A mess

    /* check extension, case insensitive */
	/* .aac: expected, .laac/ace: for players to avoid hijacking MP4/AAC */
    if ( !check_extensions(streamFile,"aac,laac,ace"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x41414320)   /* "AAC " */
        goto fail;

	/* Ok, let's check what's behind door number 1 */
	if (read_32bitBE(0x1000, streamFile) == 0x41534320) /* "ASC " */
	{
		loop_flag = read_32bitBE(0x1118, streamFile);

		/*Funky Channel Count Checking */
		if (read_32bitBE(0x1184, streamFile) == 0x7374726D)
			channel_count = 6;
		else if (read_32bitBE(0x1154, streamFile) == 0x7374726D)
			channel_count = 4;
		else
			channel_count = read_8bit(0x1134, streamFile);

		sampleRate = read_32bitBE(0x10F4, streamFile);
		numSamples = read_32bitBE(0x10FC, streamFile);
		startSample = read_32bitBE(0x10F8, streamFile);
		dataSize = read_32bitBE(0x10F0, streamFile);
		blockSize = read_32bitBE(0x1100, streamFile);
		blockCount = read_32bitBE(0x110C, streamFile);
	}
	else if (read_32bitBE(0x1000, streamFile) == 0x57415645) /* "WAVE" */
	{
		loop_flag = read_32bitBE(0x1048, streamFile);

		/*Funky Channel Count Checking */
		if (read_32bitBE(0x10B0, streamFile) == 0x7374726D)
			channel_count = 6;
		else if (read_32bitBE(0x1080, streamFile) == 0x7374726D)
			channel_count = 4;
		else
			channel_count = read_8bit(0x1060, streamFile);

		sampleRate = read_32bitBE(0x1024, streamFile);
		numSamples = read_32bitBE(0x102C, streamFile);
		startSample = read_32bitBE(0x1028, streamFile);
		dataSize = read_32bitBE(0x1020, streamFile);
		blockSize = read_32bitBE(0x1030, streamFile);
		blockCount = read_32bitBE(0x103C, streamFile);
	}
	else if (read_32bitBE(0x1000, streamFile) == 0x00000000) /* some like to be special */
	{
		loop_flag = read_32bitBE(0x6048, streamFile);

		/*Funky Channel Count Checking */
		if (read_32bitBE(0x60B0, streamFile) == 0x7374726D)
			channel_count = 6;
		else if (read_32bitBE(0x6080, streamFile) == 0x7374726D)
			channel_count = 4;
		else
			channel_count = read_8bit(0x6060, streamFile);

		sampleRate = read_32bitBE(0x6024, streamFile);
		numSamples = read_32bitBE(0x602C, streamFile);
		startSample = read_32bitBE(0x6028, streamFile);
		dataSize = read_32bitBE(0x6020, streamFile);
		blockSize = read_32bitBE(0x6030, streamFile);
		blockCount = read_32bitBE(0x603C, streamFile);
	}
	else
		goto fail; //cuz I don't know if there are other variants

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	if (read_32bitBE(0x1000, streamFile) == 0x00000000)
		start_offset = 0x7000;
	else
		start_offset = 0x2000;

    vgmstream->sample_rate = sampleRate;
    vgmstream->channels = channel_count;
    vgmstream->num_samples = numSamples;
	if (loop_flag) {
		vgmstream->loop_start_sample = startSample;
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}
    vgmstream->meta_type = meta_TA_AAC_X360;

#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data *ffmpeg_data = NULL;
        uint8_t buf[100];
        size_t bytes, datasize, block_size, block_count;

		block_count = blockCount;
		block_size = blockSize;
		datasize = dataSize;

        bytes = ffmpeg_make_riff_xma2(buf,100, vgmstream->num_samples, datasize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
        if (bytes <= 0) goto fail;

        ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,datasize);
        if ( !ffmpeg_data ) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* PlayStation 3 Variants (Star Ocean International, Resonance of Fate) */
VGMSTREAM * init_vgmstream_ta_aac_ps3(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	off_t start_offset;
	int loop_flag, channel_count;
	uint32_t data_size, loop_start, loop_end, codec_id;

	/* check extension, case insensitive */
	/* .aac: expected, .laac/ace: for players to avoid hijacking MP4/AAC */
	if (!check_extensions(streamFile, "aac,laac,ace"))
		goto fail;

	if (read_32bitBE(0x00, streamFile) != 0x41414320)   /* "AAC " */
		goto fail;

	/* Haven't Found a codec flag yet. Let's just use this for now */
	if (read_32bitBE(0x10000, streamFile) != 0x41534320)   /* "ASC " */
		goto fail;

	if (read_32bitBE(0x10104, streamFile) != 0xFFFFFFFF)
		loop_flag = 1;
	else
		loop_flag = 0;

	channel_count = read_32bitBE(0x100F4, streamFile);
	codec_id = read_32bitBE(0x100F0, streamFile);

	/* build the VGMSTREAM */
	vgmstream = allocate_vgmstream(channel_count, loop_flag);
	if (!vgmstream) goto fail;

	/* Useless header, let's play the guessing game */
	start_offset = 0x10110;
	vgmstream->sample_rate = read_32bitBE(0x100FC, streamFile);
	vgmstream->channels = channel_count;
	vgmstream->meta_type = meta_TA_AAC_PS3;
	data_size = read_32bitBE(0x100F8, streamFile);
	loop_start = read_32bitBE(0x10104, streamFile);
	loop_end = read_32bitBE(0x10108, streamFile);

#ifdef VGM_USE_FFMPEG
	{
		ffmpeg_codec_data *ffmpeg_data = NULL;
		uint8_t buf[100];
		int32_t bytes, samples_size = 1024, block_size, encoder_delay, joint_stereo, max_samples;
		block_size = (codec_id == 4 ? 0x60 : (codec_id == 5 ? 0x98 : 0xC0)) * vgmstream->channels;
		max_samples = (data_size / block_size) * samples_size;
		encoder_delay = 0x0;
		joint_stereo = 0;

        /* make a fake riff so FFmpeg can parse the ATRAC3 */
		bytes = ffmpeg_make_riff_atrac3(buf, 100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
		if (bytes <= 0) goto fail;

		ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf, bytes, start_offset, data_size);
		if (!ffmpeg_data) goto fail;
		vgmstream->codec_data = ffmpeg_data;
		vgmstream->coding_type = coding_FFmpeg;
		vgmstream->layout_type = layout_none;
		vgmstream->num_samples = max_samples;

		if (loop_flag) {
			vgmstream->loop_start_sample = (loop_start / block_size) * samples_size;
			vgmstream->loop_end_sample = (loop_end / block_size) * samples_size;
		}

	}
#endif

	/* open the file for reading */
	if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
		goto fail;
	return vgmstream;

fail:
	close_vgmstream(vgmstream);
	return NULL;
}
