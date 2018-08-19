#include "meta.h"
#include "../util.h"
#include "../layout/layout.h"

/* matx

   MATX (found in Matrix)
*/

VGMSTREAM * init_vgmstream_xbox_matx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    int loop_flag=0;
	int channel_count;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("matx",filename_extension(filename))) goto fail;

	loop_flag = 0;
	channel_count=read_16bitLE(0x4,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_16bitLE(0x06,streamFile) & 0xffff;

	vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_blocked_matx;
    vgmstream->meta_type = meta_XBOX_MATX;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

	/* Calc num_samples */
	block_update_matx(0,vgmstream);
	vgmstream->num_samples=0;

	do {
		vgmstream->num_samples += vgmstream->current_block_size/36*64;
		block_update_matx(vgmstream->next_block_offset,vgmstream);
	} while (vgmstream->next_block_offset<get_streamfile_size(streamFile));

	block_update_matx(0,vgmstream);
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
