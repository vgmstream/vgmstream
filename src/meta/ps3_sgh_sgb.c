#include "meta.h"
#include "../util.h"

/* SGH+SGB (from Folklore) */
VGMSTREAM * init_vgmstream_ps3_sgh_sgb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    STREAMFILE * streamFileSGH = NULL;
    char filename[260];
    char filenameSGH[260];
    int i;
    int channel_count;
    int loop_flag;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sgb",filename_extension(filename))) goto fail;

	strcpy(filenameSGH,filename);
    strcpy(filenameSGH+strlen(filenameSGH)-3,"sgh");

    streamFileSGH = streamFile->open(streamFile,filenameSGH,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!streamFileSGH) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFileSGH) != 0x53475844) /* "SGXD" */
        goto fail;

    channel_count = read_32bitLE(0x6C,streamFileSGH);
 	if (read_32bitBE(0x44,streamFileSGH)==0xFFFFFFFF)
        loop_flag = 0;
	else
		loop_flag = 1;
	
   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x2C,streamFileSGH);
    vgmstream->num_samples = read_32bitLE(0xC,streamFileSGH)*28/32;
    vgmstream->coding_type = coding_PSX;
    if(loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x44,streamFileSGH);
        vgmstream->loop_end_sample = read_32bitLE(0x48,streamFileSGH);
    }	


    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS3_SGH_SGB;

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
