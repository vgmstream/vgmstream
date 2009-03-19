#include "meta.h"
#include "../util.h"

/* VGS (Phantom of Inferno) */
VGMSTREAM * init_vgmstream_ps2_vgs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vgs",filename_extension(filename))) goto fail;

    /* check header */

	/* Look if there's more than 1 one file... */
    if (read_32bitBE(0x00,streamFile) != 0x56475300)
        goto fail;

	loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = 0x30;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
	vgmstream->num_samples = ((get_streamfile_size(streamFile)-0x30)/16/channel_count*28);
 //   if (loop_flag) {
 //       vgmstream->loop_start_sample = 0;
 //       vgmstream->loop_end_sample = (read_32bitLE(0x08,streamFile))/channel_count/32*28;
	//}

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x04,streamFile)*0x1000;
    vgmstream->meta_type = meta_PSX_FAG;

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
