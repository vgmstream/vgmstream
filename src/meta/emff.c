#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/*

...EMFF - Eidos Music File Format...

*/

VGMSTREAM * init_vgmstream_emff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;
	int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("emff",filename_extension(filename))) goto fail;

    /* check header */
#if 0
    if (read_32bitBE(0x00,streamFile) != 0x53565300) /* "SVS\0" */
        goto fail;
#endif

	loop_flag = (read_32bitLE(0x04,streamFile) != 0xFFFFFFFF);
    channel_count = read_32bitLE(0x0C,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x00,streamFile);
    vgmstream->coding_type = coding_PSX;
    /* vgmstream->num_samples = read_32bitLE(0x08,streamFile); */
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = read_32bitLE(0x08,streamFile);
    }

    vgmstream->layout_type = layout_emff_blocked;
    vgmstream->interleave_block_size = 0x10; 
    vgmstream->meta_type = meta_EMFF;

	/* open the file for reading */
	{
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,0x2000);
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }
	

	/* Calc num_samples */
	emff_block_update(start_offset,vgmstream);
	vgmstream->num_samples=0;

	do {
		vgmstream->num_samples += vgmstream->current_block_size*28/16/channel_count;
		emff_block_update(vgmstream->next_block_offset,vgmstream);
	} while (vgmstream->next_block_offset<get_streamfile_size(streamFile));

	emff_block_update(start_offset,vgmstream);
    	

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
