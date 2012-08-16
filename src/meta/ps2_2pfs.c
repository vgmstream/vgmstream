#include "meta.h"
#include "../util.h"

/* 2PFS
	- Mahoromatic: Moetto - KiraKira Maid-San (PS2)

	
*/
VGMSTREAM * init_vgmstream_ps2_2pfs(STREAMFILE *streamFile) 
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
    if (strcasecmp("2pfs",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x32504653) 
        goto fail;

	// channel count
	channel_count = read_8bit(0x40,streamFile);

	// header size
	start_offset = 0x800;
    
	// loop flag
	//if ((read_32bitLE(0x38, streamFile) != 0 || 
	//	(read_32bitLE(0x34, streamFile) != 0)))
	//{
	//	loop_flag = 1;
	//}

	// Loop info unknown right now
	//if (loop_flag) 
	//{
	//	vgmstream->loop_start_sample = read_32bitLE(0x38,streamFile)*28/16/channel_count;
	//	vgmstream->loop_end_sample = read_32bitLE(0x34,streamFile)*28/16/channel_count;
	//}

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */	
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x44,streamFile);
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = read_32bitLE(0x0C,streamFile)*28/16/channel_count;
 
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1000;
    vgmstream->meta_type = meta_PS2_2PFS;

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
