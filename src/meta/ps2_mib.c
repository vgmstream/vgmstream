#include "meta.h"
#include "../util.h"

/* MIB

   PS2 MIB format is a headerless format.
   The interleave value can be found by checking the body of the data.
   
   The interleave start allways at offset 0 with a int value (which can have
   many values : 0x0000, 0x0002, 0x0006 etc...) follow by 12 empty (zero) values.

   The interleave value is the offset where you found the same 16 bytes.

   The n° of channels can be found by checking each time you found this 16 bytes.

   The interleave value can be very "large" (up to 0x20000 found so far) and is allways
   a 0x10 multiply value.
   
   The loop values can be found by checking the 'tags' offset (found @ 0x02 each 0x10 bytes).
   06 = start of the loop point (can be found for each channel)
   03 - end of the loop point (can be found for each channel)

   The .MIH header contains all informations about frequency, numbers of channels, interleave
   but has, afaik, no loop values.

   known extensions : MIB (MIH for the header) MIC (concatenation of MIB+MIH)
					  Nota : the MIC stuff is not supported here as there is
							 another MIC format which can be found in Koei Games.

   2008-05-14 - Fastelbja : First version ...
   2008-05-20 - Fastelbja : Fix loop value when loopEnd==0
*/

VGMSTREAM * init_vgmstream_ps2_mib(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileMIH = NULL;
    char filename[260];
    
	uint8_t mibBuffer[0x10];
	uint8_t	testBuffer[0x10];
	uint8_t doChannelUpdate=1;
	uint8_t bDoUpdateInterleave=1;

	size_t	fileLength;
	
	off_t	loopStart = 0;
	off_t	loopEnd = 0;

	off_t	interleave = 0;

	off_t	readOffset = 0;

	char	filenameMIH[260];
	off_t	loopStartPoints[0x10];
	int		loopStartPointsCount=0;

	off_t	loopEndPoints[0x10];
	int		loopEndPointsCount=0;

	int		loopToEnd=0;
	int		forceNoLoop=0;
	int		gotEmptyLine=0;

	uint8_t gotMIH=0;

	int i, channel_count=0;

	// Initialize loop point to 0
	for(i=0; i<0x10; i++) {
		loopStartPoints[i]=0;
		loopEndPoints[i]=0;
	}

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mib",filename_extension(filename)) && 
		strcasecmp("mi4",filename_extension(filename)) && 
		strcasecmp("vb",filename_extension(filename))  &&
		strcasecmp("xag",filename_extension(filename))) goto fail;

	/* check for .MIH file */
	strcpy(filenameMIH,filename);
	strcpy(filenameMIH+strlen(filenameMIH)-3,"MIH");

	streamFileMIH = streamFile->open(streamFile,filenameMIH,STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (streamFileMIH) gotMIH = 1;

    /* Search for interleave value & loop points */
	/* Get the first 16 values */
	fileLength = get_streamfile_size(streamFile);
	
	readOffset+=(off_t)read_streamfile(mibBuffer,0,0x10,streamFile); 
	readOffset=0;
	mibBuffer[0]=0;

	do {
		readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile); 
		// be sure to point to an interleave value
		if(readOffset<(int32_t)(fileLength*0.5)) {

			if(memcmp(testBuffer+2, mibBuffer+2,0x0e)) {
				if(doChannelUpdate) {
					doChannelUpdate=0;
					channel_count++;
				}
				if(channel_count<2)
					bDoUpdateInterleave=1;
			}

			testBuffer[0]=0;
			if(!memcmp(testBuffer,mibBuffer,0x10)) {

				gotEmptyLine=1;

				if(bDoUpdateInterleave) {
					bDoUpdateInterleave=0;
					interleave=readOffset-0x10;
				}
				if(((readOffset-0x10)==(channel_count*interleave))) {
					doChannelUpdate=1;
				}
			}
		}

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
		if((testBuffer[0x01]==0x03) && (testBuffer[0x03]!=0x77)) {
			if(loopEndPointsCount<0x10) 
			{
				loopEndPoints[loopEndPointsCount] = readOffset;
				loopEndPointsCount++;
			}
		}

		if(testBuffer[0x01]==0x04) 
		{
			// 0x04 loop points flag can't be with a 0x03 loop points flag
			if(loopStartPointsCount<0x10) 
			{
				loopStartPoints[loopStartPointsCount] = readOffset-0x10;
				loopStartPointsCount++;

				// Loop end value is not set by flags ...
				// go until end of file
				loopToEnd=1;
			}
		}

	} while (streamFile->get_offset(streamFile)<((int32_t)fileLength));

	if((testBuffer[0]==0x0c) && (testBuffer[1]==0))
		forceNoLoop=1;

	if(channel_count==0)
		channel_count=1;

	if(gotMIH) 
		channel_count=read_32bitLE(0x08,streamFileMIH);

	// force no loop
	if(!strcasecmp("vb",filename_extension(filename))) 
		loopStart=0;

	if(!strcasecmp("xag",filename_extension(filename))) 
		channel_count=2;

	// Calc Loop Points & Interleave ...
	if(loopStartPointsCount>=2) 
	{
		// can't get more then 0x10 loop point !
		if(loopStartPointsCount<=0x0F) {
			// Always took the first 2 loop points
			interleave=loopStartPoints[1]-loopStartPoints[0];
			loopStart=loopStartPoints[1];

			// Can't be one channel .mib with interleave values
			if((interleave>0) && (channel_count==1)) 
				channel_count=2;
		} else 
			loopStart=0;
	}

	if(loopEndPointsCount>=2) 
	{
		// can't get more then 0x10 loop point !
		if(loopEndPointsCount<=0x0F) {
			// No need to recalculate interleave value ...
			loopEnd=loopEndPoints[loopEndPointsCount-1];

			// Can't be one channel .mib with interleave values
			if(channel_count==1) channel_count=2;
		} else {
			loopToEnd=0;
			loopEnd=0;
		}
	}

	if (loopToEnd) 
		loopEnd=fileLength;

	// force no loop 
	if(forceNoLoop) 
		loopEnd=0;

	if((interleave>0x10) && (channel_count==1))
		channel_count=2;

	if(interleave==0) interleave=0x10;

	// further check on channel_count ...
	if(gotEmptyLine) 
	{
		int newChannelCount = 0;

		readOffset=0;

		do 
		{
			newChannelCount++;
			read_streamfile(testBuffer,readOffset,0x10,streamFile); 
			readOffset+=interleave;
		} while(!memcmp(testBuffer,mibBuffer,16));

		newChannelCount--;

		if(newChannelCount>channel_count)
			channel_count=newChannelCount;
	}

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,(loopEnd!=0));
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;

	if(gotMIH) {
		// Read stuff from the MIH file 
		vgmstream->channels = read_32bitLE(0x08,streamFileMIH);
		vgmstream->sample_rate = read_32bitLE(0x0C,streamFileMIH);
		vgmstream->interleave_block_size = read_32bitLE(0x10,streamFileMIH);
		vgmstream->num_samples=((read_32bitLE(0x10,streamFileMIH)*
								(read_32bitLE(0x14,streamFileMIH)-1)*2)+
								((read_32bitLE(0x04,streamFileMIH)>>8)*2))/16*28/2;
	} else {
		vgmstream->channels = channel_count;
		vgmstream->interleave_block_size = interleave;

		if(!strcasecmp("mib",filename_extension(filename))) 
			vgmstream->sample_rate = 44100;

		if(!strcasecmp("mi4",filename_extension(filename)))
			vgmstream->sample_rate = 48000;

		if(!strcasecmp("xag",filename_extension(filename))) {
			vgmstream->channels=2;
			vgmstream->sample_rate = 44100;
		}

		if(!strcasecmp("vb",filename_extension(filename))) 
		{
			vgmstream->layout_type = layout_none;
			vgmstream->interleave_block_size=0;
			vgmstream->sample_rate = 22050;
			vgmstream->channels = 1;
			vgmstream->start_ch = 1;
			vgmstream->loop_ch = 1;
		}

		vgmstream->num_samples = (int32_t)(fileLength/16/channel_count*28);
	}

	if(loopEnd!=0) {
		if(vgmstream->channels==1) {
			vgmstream->loop_start_sample = loopStart/16*18;
			vgmstream->loop_end_sample = loopEnd/16*28;
		} else {
			vgmstream->loop_start_sample = ((((loopStart/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*14*channel_count)/channel_count;
			if(loopStart%vgmstream->interleave_block_size) {
				vgmstream->loop_start_sample += (((loopStart%vgmstream->interleave_block_size)-1)/16*14*channel_count);
			}

			if(loopEnd==fileLength) 
			{
				vgmstream->loop_end_sample=(loopEnd/16*28)/channel_count;
			} else {
				vgmstream->loop_end_sample = ((((loopEnd/vgmstream->interleave_block_size)-1)*vgmstream->interleave_block_size)/16*14*channel_count)/channel_count;

				if(loopEnd%vgmstream->interleave_block_size) {
					vgmstream->loop_end_sample += (((loopEnd%vgmstream->interleave_block_size)-1)/16*14*channel_count);
				}
			}
		}
	}

	if(loopToEnd) 
	{
		// try to find if there's no empty line ...
		int emptySamples=0;

		for(i=0; i<16;i++) {
			mibBuffer[i]=0;
		}

		readOffset=fileLength-0x10;

		do {
			read_streamfile(testBuffer,readOffset,0x10,streamFile); 
			if(!memcmp(mibBuffer,testBuffer,16)) 
			{
				emptySamples+=28;
			}
			readOffset-=0x10;
		} while(!memcmp(testBuffer,mibBuffer,16));

		vgmstream->loop_end_sample-=(emptySamples*channel_count);
	}
	vgmstream->meta_type = meta_PS2_MIB;
    
	if (gotMIH) {
		vgmstream->meta_type = meta_PS2_MIB_MIH;
		close_streamfile(streamFileMIH); streamFileMIH=NULL;
	}

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,0x8000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileMIH) close_streamfile(streamFileMIH);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
