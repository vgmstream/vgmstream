#include "meta.h"
#include "../util.h"

//#include <windows.h>
//#include <tchar.h>

/* VBK (from Disney's Stitch - Experiment 626) */

VGMSTREAM * init_vgmstream_ps2_vbk(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	off_t start_offset;
	uint8_t	testBuffer[0x10];
	off_t	loopStart = 0;
	off_t	loopEnd = 0;
	off_t	readOffset = 0;
	size_t	fileLength;
	int loop_flag;
	int channel_count;

    //_TCHAR szBuffer[100];

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vbk",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x2E56424B) /* .VBK */
        goto fail;

    loop_flag = 1;
    channel_count = read_32bitLE(0x28,streamFile) + 1;

	//_stprintf(szBuffer, _T("%x"), channel_count);
	//MessageBox(NULL, szBuffer, _T("Foo"), MB_OK);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    fileLength = get_streamfile_size(streamFile);
	start_offset = read_32bitLE(0x0C, streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x20,streamFile);
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = (fileLength - start_offset)*28/16/channel_count;
		
	// get loop start
	do {
		
		readOffset+=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile); 

		if(testBuffer[0x01]==0x06) 
		{
			loopStart = readOffset-0x10;
			break;
		}

	} while (streamFile->get_offset(streamFile)<(int32_t)fileLength);
	
	
	// get loop end
	readOffset = fileLength - 0x10;
	
	do {		
		readOffset-=(off_t)read_streamfile(testBuffer,readOffset,0x10,streamFile); 

		/* Loop End */
		if(testBuffer[0x01]==0x03) 
		{
			loopEnd = readOffset-0x10;
			break;
		}
	} while (readOffset > 0);

	loop_flag = 1;
	vgmstream->loop_start_sample = (loopStart-start_offset)*28/16/channel_count;
    vgmstream->loop_end_sample = (loopEnd-start_offset)*28/16/channel_count;

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x24,streamFile);
    vgmstream->meta_type = meta_PS2_VBK;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        
		for (i=0;i<channel_count;i++) 
		{
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

