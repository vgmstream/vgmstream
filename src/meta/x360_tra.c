#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* TRA

   TRA is an headerless format which can be found on DefJam Rapstar (X360)
   known extensions : WAVM

   2010-12-03 - Fastelbja : First version ...
*/
VGMSTREAM * init_vgmstream_x360_tra(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    int loop_flag=0;
	int channel_count;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("tra",filename_extension(filename))) goto fail;

    /* No loop on wavm */
	loop_flag = 0;
    
	/* Always stereo files */
	channel_count=2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	/* allways 2 channels @ 44100 Hz */
	vgmstream->channels = 2;
    vgmstream->sample_rate = 24000;

	vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->num_samples = (int32_t)(get_streamfile_size(streamFile) - ((get_streamfile_size(streamFile)/0x204)*4));
    vgmstream->layout_type = layout_blocked_tra;
	
    vgmstream->meta_type = meta_X360_TRA;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

	block_update_tra(0,vgmstream);
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
