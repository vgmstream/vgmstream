#include "meta.h"
#include "../util.h"

/* Stuff from NUB archives */

/* VAG (from Ridge Racer 7) */
VGMSTREAM * init_vgmstream_nub_vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("vag",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x76616700) /* "vag" */
        goto fail;

	if (read_32bitBE(0x30,streamFile)==0x3F800000)
		loop_flag = 1;
	else 
		loop_flag = 0;
	
	channel_count = 1;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0xC0;
   vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0xBC,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitBE(0x14,streamFile)*28/32*2;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x20,streamFile)*28/32*2;
       vgmstream->loop_end_sample = read_32bitBE(0x24,streamFile)*28/32*2;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NUB_VAG;

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
