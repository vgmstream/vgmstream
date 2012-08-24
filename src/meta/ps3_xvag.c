#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* XVAG (from Ratchet & Clank Future: Quest for Booty) */
/* v0.1 : bxaimc : Initial release					   */
/* v0.2 : Fastelbja : add support for loops points	   */
/*                    + little endian header values    */

VGMSTREAM * init_vgmstream_ps3_xvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
	uint8_t	testBuffer[0x10];

    off_t start_offset;
	off_t fileLength;

    int loop_flag = 0;
    int channel_count;
	off_t	readOffset = 0;
	int little_endian = 0;
    long sample_rate = 0;
    long num_samples = 0;

#ifdef VGM_USE_MPEG
    mpeg_codec_data *mpeg_data = NULL;
    coding_t mpeg_coding_type = coding_MPEG1_L3;
#endif

	int		loopStartPointsCount=0;
	int		loopEndPointsCount=0;
	uint16_t mp3ID;
	off_t	loopStartPoints[0x10];
	off_t	loopEndPoints[0x10];

	off_t	loopStart = 0;
	off_t	loopEnd = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xvag",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
        goto fail;

	fileLength = get_streamfile_size(streamFile);

	if (read_8bit(0x07,streamFile)==0)
		little_endian=1;

	if(little_endian)
	{
		channel_count = read_32bitLE(0x28,streamFile);
		start_offset = read_32bitLE(0x4,streamFile);
		sample_rate = read_32bitLE(0x3c,streamFile);
	    num_samples = read_32bitLE(0x30,streamFile);
	}
	else
	{
		channel_count = read_32bitBE(0x28,streamFile);
		start_offset = read_32bitBE(0x4,streamFile);
		sample_rate = read_32bitBE(0x3c,streamFile);
	    num_samples = read_32bitBE(0x30,streamFile);
	}

	readOffset=start_offset;

	// MP3s ?
	mp3ID=(uint16_t)read_16bitBE(start_offset,streamFile);
	if(mp3ID==0xFFFB) {
#ifdef VGM_USE_MPEG
        int rate;
        int channels;

        mpeg_data = init_mpeg_codec_data(streamFile, start_offset, -1, -1, &mpeg_coding_type, &rate, &channels); // -1 to not check sample rate or channels
        if (!mpeg_data) goto fail;

        channel_count = channels;
        sample_rate = rate;

#else
        // reject if no MPEG support
        goto fail;
#endif
    } else {
		// get the loops the same way we get on .MIB
		do {
			readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile); 

			// Loop Start ...
			if(testBuffer[0x01]==0x06) 
			{
				if(loopStartPointsCount<0x10) 
				{
					loopStartPoints[loopStartPointsCount] = readOffset-0x10;
					loopStartPointsCount++;
				}
			}

			// Loop End ...
			if(((testBuffer[0x01]==0x03) && (testBuffer[0x03]!=0x77)) ||
				(testBuffer[0x01]==0x01)) 
			{
				if(loopEndPointsCount<0x10) 
				{
					loopEndPoints[loopEndPointsCount] = readOffset; //-0x10;
					loopEndPointsCount++;
				}

			}

		} while (readOffset<((int32_t)fileLength));

			// Calc Loop Points & Interleave ...
		if(loopStartPointsCount>=channel_count) 
		{
			// can't get more then 0x10 loop point !
			if((loopStartPointsCount<=0x0F) && (loopStartPointsCount>=2)) 
			{
				// Always took the first 2 loop points
				loopStart=loopStartPoints[1]-start_offset;
				loop_flag=1;
			} else 
				loopStart=0;
		}

		if(loopEndPointsCount>=channel_count) 
		{
			// can't get more then 0x10 loop point !
			if((loopEndPointsCount<=0x0F) && (loopEndPointsCount>=2)) {
				loop_flag=1;
				loopEnd=loopEndPoints[loopEndPointsCount-1]-start_offset;
			} else {
				loopEnd=0;
			}
		}

		// as i can get on the header if a song is looped or not
		// if try to take the loop marker on the file
		// if the last byte of the file is = 00 is assume that the song is not looped
		// i know that i can cover 95% of the file, but can't work on some of them
		if(read_8bit((fileLength-1),streamFile)==0)
			loop_flag=0;
	}

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->channels = channel_count;
    vgmstream->meta_type = meta_PS3_XVAG;
	
	if(mp3ID==0xFFFB) {
#ifdef VGM_USE_MPEG
        /* NOTE: num_samples seems to be quite wrong for MPEG */
        vgmstream->codec_data = mpeg_data;
		vgmstream->layout_type = layout_mpeg;
		vgmstream->coding_type = mpeg_coding_type;
#else
        // reject if no MPEG support
        goto fail;
#endif
	}
	else {
	    vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x10;
		vgmstream->coding_type = coding_PSX;
	}

	if (loop_flag) {
		if(loopStart!=0) {
			vgmstream->loop_start_sample = ((((loopStart/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*28)/channel_count;
			if(loopStart%vgmstream->interleave_block_size) {
				vgmstream->loop_start_sample += (((loopStart%vgmstream->interleave_block_size)-1)/16*14*channel_count);
			}
		}
       vgmstream->loop_end_sample = ((((loopEnd/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*28)/channel_count;
		if(loopEnd%vgmstream->interleave_block_size) {
			vgmstream->loop_end_sample += (((loopEnd%vgmstream->interleave_block_size)-1)/16*14*channel_count);
		}
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
		if(vgmstream->layout_type == layout_interleave) {
			file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
			if (!file) goto fail;
			for (i=0;i<channel_count;i++) {
				vgmstream->ch[i].streamfile = file;

				vgmstream->ch[i].channel_start_offset=
					vgmstream->ch[i].offset=start_offset+
					vgmstream->interleave_block_size*i;

			}
		}
#ifdef VGM_USE_MPEG
		else if(vgmstream->layout_type == layout_mpeg) {
			for (i=0;i<channel_count;i++) {
				vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,MPEG_BUFFER_SIZE);
				vgmstream->ch[i].channel_start_offset= vgmstream->ch[i].offset=start_offset;
			}

        }
#endif
        else { goto fail; }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
#ifdef VGM_USE_MPEG
    if (mpeg_data) {
        mpg123_delete(mpeg_data->m);
        free(mpeg_data);

        if (vgmstream) {
            vgmstream->codec_data = NULL;
        }
    }
#endif
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
