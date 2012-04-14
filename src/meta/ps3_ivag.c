#include "meta.h"
#include "../util.h"

/* IVAG
	- The Idolm@ster: Gravure For You! Vol. 3 (PS3)

	Appears to be two VAGp streams interleaved.
*/
VGMSTREAM * init_vgmstream_ps3_ivag(STREAMFILE *streamFile) 
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
    if (strcasecmp("ivag",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x49564147) // "IVAG"
        goto fail;

	// channel count
	channel_count = read_32bitBE(0x08, streamFile);

	// header size
	start_offset = 0x40 + (0x40 * channel_count);
    
	// loop flag
	if ((read_32bitBE(0x14, streamFile) != 0 || 
		(read_32bitBE(0x18, streamFile) != 0)))
	{
		loop_flag = 1;
	}

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */	
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x0C,streamFile);
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = read_32bitBE(0x10,streamFile);
 
	if (loop_flag) 
	{
		vgmstream->loop_start_sample = read_32bitBE(0x14,streamFile);
		vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile);
	}

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x1C,streamFile);
    vgmstream->meta_type = meta_PS3_IVAG;

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
