#include "meta.h"
#include "../util.h"

/* SEG (found in Eragon) */
VGMSTREAM * init_vgmstream_seg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
	int loop_flag;
	int channel_count;
    coding_t coding;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("seg",filename_extension(filename))) goto fail;


    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x73656700)    /* "seg\0" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) == 0x70733200)    /* "ps2\0" */
    {
        coding = coding_PSX;
    }
    else if (read_32bitBE(0x04,streamFile) == 0x78627800)   /* "xbx\0" */
    {
        coding = coding_XBOX;
    }
    else goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x24,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x4000;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);
    vgmstream->coding_type = coding;

    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x1C,streamFile);
    }

    vgmstream->interleave_block_size = 0;

    if (coding_PSX == coding)
    {
        vgmstream->num_samples = (read_32bitLE(0x0C,streamFile)-start_offset)*28/16/channel_count;
        vgmstream->meta_type = meta_PS2_SEG;

	    if (channel_count == 1) {
		    vgmstream->layout_type = layout_none;
	    } else if (channel_count == 2) {
		    vgmstream->layout_type = layout_interleave;
		    vgmstream->interleave_block_size = 0x2000;
	    }
    }
    else if (coding_XBOX == coding)
    {
        vgmstream->num_samples = (read_32bitLE(0x0C,streamFile)-start_offset)/36/channel_count*64;
        vgmstream->meta_type = meta_XBOX_SEG;
        vgmstream->layout_type = layout_none;
    }
    else goto fail;

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
