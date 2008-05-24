#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* WAVM

   WAVM is an headerless format which can be found on XBOX
   known extensions : WAVM

   2008-05-23 - Fastelbja : First version ...
*/

VGMSTREAM * init_vgmstream_xbox_wavm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    int loop_flag=0;
	int channel_count;
    off_t start_offset;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wavm",filename_extension(filename))) goto fail;

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
    vgmstream->sample_rate = 44100;

	vgmstream->coding_type = coding_XBOX;
    vgmstream->num_samples = get_streamfile_size(streamFile) / 36 * 64 / vgmstream->channels;
    vgmstream->layout_type = layout_xbox_blocked;
	vgmstream->current_block_size=36*vgmstream->channels;
	vgmstream->current_block_offset=0;
	
	xbox_block_update(0,vgmstream);

    vgmstream->meta_type = meta_XBOX_WAVM;

	start_offset = 0;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,36);

            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
