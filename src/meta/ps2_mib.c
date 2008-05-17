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
*/

VGMSTREAM * init_vgmstream_ps2_mib(const char * const filename) {
    
	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
	STREAMFILE * infileMIH = NULL;

	uint8_t mibBuffer[0x10];
	uint8_t	testBuffer[0x10];

	size_t	fileLength;
	
	off_t	loopStart = 0;
	off_t	loopEnd = 0;
	off_t	interleave = 0;
	off_t	readOffset = 0;

	char * filenameMIH = NULL;

	uint8_t gotMIH=0;

	int i, channel_count=1;

    /* check extension, case insensitive */
    if (strcasecmp("mib",filename_extension(filename)) && 
		strcasecmp("mi4",filename_extension(filename))) goto fail;

	/* check for .MIH file */
	filenameMIH=(char *)malloc(strlen(filename)+1);

	if (!filenameMIH) goto fail;

	strcpy(filenameMIH,filename);
	strcpy(filenameMIH+strlen(filenameMIH)-3,"MIH");

	infileMIH = open_streamfile(filenameMIH);
	if (infileMIH) gotMIH = 1;

    free(filenameMIH); filenameMIH = NULL;

	/* Search for interleave value & loop points */
	/* Get the first 16 values */
	infile=open_streamfile_buffer(filename,0x8000);

	if(!infile) goto fail;

	fileLength = get_streamfile_size(infile);
	
	readOffset+=read_streamfile(mibBuffer,0,0x10,infile); 

	do {
		readOffset+=read_streamfile(testBuffer,readOffset,0x10,infile); 
		
		if(!memcmp(testBuffer,mibBuffer,0x10)) {
			if(interleave==0) interleave=readOffset-0x10;

			// be sure to point to an interleave value
			if(((readOffset-0x10)==channel_count*interleave)) {
				channel_count++;
			}
		}

		// Loop Start ...
		if(testBuffer[0x01]==0x06) {
			if(loopStart==0) loopStart = readOffset-0x10;
		}

		// Loop End ...
		if(testBuffer[0x01]==0x03) {
			if(loopEnd==0) loopEnd = readOffset-0x10;
		}

	} while (infile->offset<fileLength);

	if(gotMIH) 
		channel_count=read_32bitLE(0x08,infileMIH);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,(loopStart!=0));
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	if(gotMIH) {
		// Read stuff from the MIH file 
		vgmstream->channels = read_32bitLE(0x08,infileMIH);
		vgmstream->sample_rate = read_32bitLE(0x0C,infileMIH);
		vgmstream->interleave_block_size = read_32bitLE(0x10,infileMIH);
		vgmstream->num_samples=((read_32bitLE(0x10,infileMIH)*
								(read_32bitLE(0x14,infileMIH)-1)*2)+
								((read_32bitLE(0x04,infileMIH)>>8)*2))/16*28/2;
	} else {
		vgmstream->channels = channel_count;
		vgmstream->interleave_block_size = interleave;

		if(!strcasecmp("mib",filename_extension(filename)))
			vgmstream->sample_rate = 44100;

		if(!strcasecmp("mi4",filename_extension(filename)))
			vgmstream->sample_rate = 48000;

		vgmstream->num_samples = fileLength/16/channel_count*28;
	}

	if(loopStart!=0) {
		vgmstream->loop_start_sample = ((loopStart/(interleave*channel_count))*interleave)/16*14*channel_count;
		vgmstream->loop_start_sample += (loopStart%(interleave*channel_count))/16*14*channel_count;
		vgmstream->loop_end_sample = ((loopEnd/(interleave*channel_count))*interleave)/16*14*channel_count;
		vgmstream->loop_end_sample += (loopEnd%(interleave*channel_count))/16*14*channel_count;
	}

	vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    
	vgmstream->meta_type = meta_PS2_MIB;
    close_streamfile(infile); infile=NULL;

	if (gotMIH) {
		vgmstream->meta_type = meta_PS2_MIB_MIH;
		close_streamfile(infileMIH); infileMIH=NULL;
	}

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x8000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (infileMIH) close_streamfile(infileMIH);
    if (filenameMIH) {free(filenameMIH); filenameMIH=NULL;}
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
