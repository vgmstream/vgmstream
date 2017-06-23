#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* ivaud (from GTA IV (PC)) */
VGMSTREAM * init_vgmstream_ivaud(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;

	char filename[PATH_LIMIT];
    off_t start_offset;
	off_t block_table_offset;
    int loop_flag = 0;
	int channel_count;
    int i;


    /* at this time, i only check for extension */
	/* i'll make further checks later */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ivaud",filename_extension(filename))) goto fail;

	/* multiple sounds .ivaud files are not implemented */
	/* only used for voices & sfx */
	if(read_32bitLE(0x10,streamFile)!=0)
		goto fail;

	/* never looped and allways 2 channels */
    loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    block_table_offset = read_32bitLE(0,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(block_table_offset + 0x04,streamFile);
    vgmstream->coding_type = coding_IMA_int;

	vgmstream->layout_type = layout_ivaud_blocked;
	vgmstream->meta_type = meta_PC_IVAUD;

	/* open the file for reading */
	{
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,0x2000);
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }
	
	/* Calc num_samples */
	start_offset = read_32bitLE(0x2C,streamFile);
	//block_count = read_32bitLE(0x08,streamFile);
	vgmstream->next_block_offset = read_32bitLE(0x2C,streamFile);

	// to avoid troubles with "extra" samples
	vgmstream->num_samples=((read_32bitLE(0x60,streamFile)/2)*2);

	ivaud_block_update(start_offset,vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
