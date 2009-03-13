#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_wii_str(STREAMFILE *streamFile) {
    
	VGMSTREAM * vgmstream = NULL;
	STREAMFILE * infileSTH = NULL;
	char filename[260];

	char * filenameSTH = NULL;

	int i, j, channel_count, loop_flag;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("str",filename_extension(filename))) goto fail;

	/* check for .MIH file */
	filenameSTH=(char *)malloc(strlen(filename)+1);

	if (!filenameSTH) goto fail;

	strcpy(filenameSTH,filename);
	strcpy(filenameSTH+strlen(filenameSTH)-3,"sth");

	infileSTH = streamFile->open(streamFile,filenameSTH,STREAMFILE_DEFAULT_BUFFER_SIZE);

	/* STH File is necessary, so we can't confuse those file */
	/* with others .STR file as it is a very common extension */
	if (!infileSTH) goto fail;

	if(read_32bitLE(0x2C,infileSTH)!=0) 
		goto fail;

	channel_count = read_32bitBE(0x70,infileSTH);

	if(channel_count==1)
		loop_flag = (read_32bitBE(0xD4,infileSTH)==0x00740000);
	else
		loop_flag = (read_32bitBE(0x124,infileSTH)==0x00740000);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->channels = channel_count;
	vgmstream->sample_rate = read_32bitBE(0x38,infileSTH);

	vgmstream->interleave_block_size=0x8000;
	vgmstream->num_samples=read_32bitBE(0x34,infileSTH);

	vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    
	vgmstream->meta_type = meta_WII_STR;

	if(loop_flag) {
		vgmstream->loop_start_sample = 0;
		vgmstream->loop_end_sample = vgmstream->num_samples;
	}


    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,vgmstream->interleave_block_size);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset+=(off_t)(vgmstream->interleave_block_size*i);

			for(j=0; j<16; j++) {
				vgmstream->ch[i].adpcm_coef[j]=read_16bitBE(0xAC+(j*2)+(i*0x50),infileSTH);
			}
        }
    }

	close_streamfile(infileSTH); infileSTH=NULL;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infileSTH) close_streamfile(infileSTH);
    if (filenameSTH) {free(filenameSTH); filenameSTH=NULL;}
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
