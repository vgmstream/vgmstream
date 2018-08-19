#include "meta.h"
#include "../util.h"

//#include <windows.h>
//#include <tchar.h>

/* WMUS - Arbitrary extension chosen for The Warriors (PS2) */

VGMSTREAM * init_vgmstream_ps2_wmus(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    int loop_flag = 1;
	int channel_count;
    off_t start_offset;
    int i;
	
	int blockCount;
	int shortBlockSize;
	int lastBlockLocation;

	char	filenameWHED[PATH_LIMIT];
	STREAMFILE * streamFileWHED = NULL;

	//_TCHAR szBuffer[100];

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    
	if (strcasecmp("wmus",filename_extension(filename))) 
	{
		goto fail;
	}
	
	/* check for .WHED file */
	strcpy(filenameWHED, filename);
	strcpy(filenameWHED + strlen(filenameWHED) - 4, "WHED");

	streamFileWHED = streamFile->open(streamFile, filenameWHED, STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (!streamFileWHED)
	{
		goto fail;
	}

	/* check loopand channel */
	loop_flag = 1;
    channel_count = read_32bitLE(0x14, streamFileWHED);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) 
	{
		goto fail;
	}

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x04, streamFileWHED);

	vgmstream->coding_type = coding_PSX;
    
	vgmstream->interleave_block_size = read_32bitLE(0x18, streamFileWHED);
	blockCount = read_32bitLE(0x1C, streamFileWHED) * channel_count;
	shortBlockSize = read_32bitLE(0x20, streamFileWHED);

	vgmstream->num_samples = (vgmstream->interleave_block_size * blockCount) / 16 / channel_count * 28;
	vgmstream->loop_start_sample = 0;
	
	lastBlockLocation = (vgmstream->interleave_block_size * blockCount) - (vgmstream->interleave_block_size - shortBlockSize);
	vgmstream->loop_end_sample = lastBlockLocation / 16 / channel_count * 28;

	//_stprintf(szBuffer, _T("%x"), lastBlockLocation);
	//MessageBox(NULL, szBuffer, _T("Foo"), MB_OK);


    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_WMUS;

	start_offset = 0;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                (off_t)(start_offset+vgmstream->interleave_block_size*i);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
	if (streamFileWHED) close_streamfile(streamFileWHED);
	if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
