#include "meta.h"
#include "../util.h"

/* U-Sing (Wii) .myspd */

VGMSTREAM * init_vgmstream_myspd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int channel_count;
    int loop_flag = 0;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("myspd",filename_extension(filename))) goto fail;

    channel_count = 2;
    start_offset = 0x20;

    /* check size */
	if ((read_32bitBE(0x0,streamFile)*channel_count+start_offset) != get_streamfile_size(streamFile))
		goto fail;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->num_samples = read_32bitBE(0x0,streamFile) * 2;
    vgmstream->sample_rate = read_32bitBE(0x4,streamFile);

	vgmstream->coding_type = coding_IMA;
    vgmstream->meta_type = meta_MYSPD;
    vgmstream->layout_type = layout_none;

    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[0].streamfile = file;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;

        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[1].streamfile = file;
        
        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[1].offset=start_offset + read_32bitBE(0x0,streamFile);
    }
    
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
