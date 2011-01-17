#include "meta.h"
#include "../util.h"

/* VAWX
	- No More Heroes: Heroes Paradise (PS3)
*/
VGMSTREAM * init_vgmstream_ps3_vawx(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    
	size_t fileLength;
	off_t readOffset = 0;
	off_t start_offset;

	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vawx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x56415758) // "VAWX"
        goto fail;

	if (read_8bit(0xF,streamFile) == 2)
	{
		loop_flag = 1;
	}

    channel_count = read_8bit(0x39,streamFile);;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */	
	start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x40,streamFile);
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = ((get_streamfile_size(streamFile)-start_offset)/16/channel_count*28);
 
	if (loop_flag) 
	{
		vgmstream->loop_start_sample = read_32bitBE(0x44,streamFile);
		vgmstream->loop_end_sample = read_32bitBE(0x48,streamFile);;
	}

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS3_VAWX;

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
                vgmstream->ch[i].offset=start_offset + (vgmstream->interleave_block_size * i);

        }
		
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
