#include "meta.h"
#include "../util.h"

/* bxaimc - 2009-03-05
	- RRDS - found in Ridge Racer DS */

VGMSTREAM * init_vgmstream_nds_rrds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int channel_count;
    int loop_flag;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rrds",filename_extension(filename))) goto fail;

    /* check size */
	if ((read_32bitLE(0x0,streamFile)+0x18) != get_streamfile_size(streamFile))
		goto fail;

    /* check type details */
    loop_flag = (read_32bitLE(0x14,streamFile) != 0);
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x1c;
	vgmstream->num_samples = (read_32bitLE(0x0,streamFile)-start_offset) / channel_count * 2;
    vgmstream->sample_rate = read_32bitLE(0x8,streamFile);

	if (loop_flag) {
		vgmstream->loop_start_sample = (read_32bitLE(0x14,streamFile)-start_offset) / channel_count * 2;
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}
	
	vgmstream->coding_type = coding_IMA;
    vgmstream->meta_type = meta_NDS_RRDS;
    vgmstream->layout_type = layout_none;


    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[0].streamfile = file;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;
        /* one NDS IMA header for whole stream */
        vgmstream->ch[0].adpcm_history1_16 = read_16bitLE(0x18,streamFile);
        vgmstream->ch[0].adpcm_step_index = read_16bitLE(0x1a,streamFile);
        if (vgmstream->ch[0].adpcm_step_index < 0 || vgmstream->ch[0].adpcm_step_index > 88) goto fail;
    }
    
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
