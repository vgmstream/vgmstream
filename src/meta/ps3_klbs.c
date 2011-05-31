#include "meta.h"
#include "../util.h"

/* .KLBS (L@VE ONCE PS3 */
VGMSTREAM * init_vgmstream_ps3_klbs(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    
	size_t fileLength;
	off_t readOffset = 0;
	off_t start_offset;
	off_t loop_start_offset;
	off_t loop_end_offset;

	uint8_t	testBuffer[0x10];
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bnk",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x20,streamFile) != 0x6B6C4253)
        goto fail;

	// get file length
	fileLength = get_streamfile_size(streamFile);	

	// Find loop start
	start_offset = read_32bitBE(0x10,streamFile);
	readOffset = start_offset;

	do {
		readOffset += (off_t)read_streamfile(testBuffer, readOffset, 0x10, streamFile); 
		
		// Loop Start ...
		if(testBuffer[0x01] == 0x06) 
		{
			loop_start_offset = readOffset - 0x10;	
			break;
		}

	} while (streamFile->get_offset(streamFile)<((int32_t)fileLength));

	// start at last line of file and move up
	readOffset = (int32_t)fileLength - 0x10;
	
	// Find loop end
	do {
		readOffset -= (off_t)read_streamfile(testBuffer, readOffset, 0x10, streamFile); 
		
		// Loop End ...
		if((testBuffer[0x01]==0x03) && (testBuffer[0x03]!=0x77)) 
		{
			loop_end_offset = readOffset +  0x20;
			break;
		}
	} while (readOffset > 0);

	// setup loops
	if (loop_start_offset > 0)
	{
		loop_flag = 1;
		
		// if we have a start loop, use EOF if end loop is not found
		if (loop_end_offset == 0)
		{
			loop_end_offset = (int32_t)fileLength - 0x10;
		}
	}
	else
	{
		loop_flag = 0;
	}

    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */		
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x90, streamFile);
    vgmstream->meta_type = meta_PS3_KLBS;

	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = ((vgmstream->interleave_block_size * channel_count)/16/channel_count*28);
 
	if (loop_flag) 
	{
		vgmstream->loop_start_sample = loop_start_offset/16/channel_count*28;
		vgmstream->loop_end_sample = loop_end_offset/16/channel_count*28;
	}

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
