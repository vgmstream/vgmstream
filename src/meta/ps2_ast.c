#include "meta.h"
#include "../util.h"

/* AST */
VGMSTREAM * init_vgmstream_ps2_ast(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag = 0;
	int channel_count;
	int variant_type;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ast",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41535400) /* "AST\0" */
        goto fail;

	/* determine variant */
	if (read_32bitBE(0x10,streamFile) == 0)
	{
		variant_type = 1;
	}
	else
	{
		variant_type = 2;
	}


    loop_flag = 0;    
    channel_count = 2;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	if (variant_type == 1)
	{
		start_offset = 0x800;
		channel_count = 2;
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile)-start_offset)*28/16/channel_count;
		vgmstream->interleave_block_size = read_32bitLE(0x08,streamFile);
		loop_flag = 0;
	}
	else if (variant_type == 2)
	{
		start_offset = 0x100;
		channel_count = read_32bitLE(0x0C,streamFile);
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
		vgmstream->num_samples = (read_32bitLE(0x04,streamFile)-start_offset)*28/16/channel_count;
		vgmstream->interleave_block_size = read_32bitLE(0x10,streamFile);
	}

    vgmstream->layout_type = layout_interleave;    
	vgmstream->coding_type = coding_PSX;
	vgmstream->meta_type = meta_PS2_AST;

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
