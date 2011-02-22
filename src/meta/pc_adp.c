#include "meta.h"
#include "../util.h"

/* ADP (from Balls of Steel) */
VGMSTREAM * init_vgmstream_bos_adp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("adp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41445021) /* "ADP!" */
        goto fail;

    loop_flag = (-1 != read_32bitLE(0x08,streamFile));
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x18;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->coding_type = coding_DVI_IMA;
    vgmstream->num_samples = read_32bitLE(0x04,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_BOS_ADP;

    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        vgmstream->ch[0].streamfile = file;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;

        // 0x10, 0x12 - both initial history?
        //vgmstream->ch[0].adpcm_history1_32 = read_16bitLE(0x10,streamFile);
        // 0x14 - initial step index?
        //vgmstream->ch[0].adpcm_step_index = read_32bitLE(0x14,streamFile);
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
