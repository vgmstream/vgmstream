#include "meta.h"
#include "../util.h"

/* ASD (found in Miss Moonlight) */
VGMSTREAM * init_vgmstream_dc_asd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
		int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("asd",filename_extension(filename))) goto fail;

	/* check header */
    if (read_32bitBE(0x20,streamFile) != 0x00000000 &&
		read_32bitBE(0x24,streamFile) != 0x00000000)
	goto fail;
		
    loop_flag = 0;
    channel_count = read_16bitLE(0x0A,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = read_32bitLE(0x0,streamFile)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x0,streamFile)/2/channel_count;
    }
	
	switch (channel_count) {
		case 1:
		vgmstream->layout_type = layout_none;
	break;
		case 2:
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x2;
	break;
		default:
			goto fail;
	}

    vgmstream->meta_type = meta_DC_ASD;

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
