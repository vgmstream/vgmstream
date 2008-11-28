#include "meta.h"
#include "../util.h"

/* GSP+GSB

   2008-11-28 - manakoAT
*/

VGMSTREAM * init_vgmstream_gsp_gsb(STREAMFILE *streamFile) {

	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileGSP = NULL;
    char filename[260];
	char filenameGSP[260];
	
	int i;
	int channel_count;
	int loop_flag;
	int header_len;
	int coef1_start;
	int coef2_start;
	int dsp_blocks;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("gsb",filename_extension(filename))) goto fail;


	strcpy(filenameGSP,filename);
	strcpy(filenameGSP+strlen(filenameGSP)-3,"GSP");

	streamFileGSP = streamFile->open(streamFile,filenameGSP,STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (!streamFileGSP) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFileGSP) != 0x47534E44)	/* "GSND" */
		goto fail;

	channel_count = (uint16_t)read_16bitBE(0x3A,streamFileGSP);
	loop_flag = 0; /* read_32bitBE(0x20,streamFileGSP); */
	header_len = read_32bitBE(0x1C,streamFileGSP);
	
	coef1_start = read_32bitBE(header_len-0x4C,streamFileGSP);
	coef2_start = read_32bitBE(header_len-0x1C,streamFileGSP);
	dsp_blocks = read_32bitBE(header_len-0x5C,streamFileGSP);
	
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->channels = channel_count;
	vgmstream->sample_rate = read_32bitBE(0x34,streamFileGSP);
	vgmstream->num_samples=read_32bitBE(0x2C,streamFileGSP)*14/8/channel_count;
	vgmstream->coding_type = coding_NGC_DSP;
	
	if(loop_flag) {
		vgmstream->loop_start_sample = 0; /* read_32bitBE(0x20,streamFileGSP)*14/8/channel_count; */
		vgmstream->loop_end_sample = read_32bitBE(0x48,streamFileGSP)*14/8/channel_count;
	}	

	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count == 2) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = read_32bitBE(header_len-0x64,streamFileGSP);
	}



    vgmstream->meta_type = meta_GSP_GSB;
	
    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

    if (!vgmstream->ch[0].streamfile) goto fail;

    vgmstream->ch[0].channel_start_offset=0;

    if (channel_count == 2) {
        vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

        if (!vgmstream->ch[1].streamfile) goto fail;

        vgmstream->ch[1].channel_start_offset=vgmstream->interleave_block_size;
    }


	if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(coef1_start+i*2,streamFileGSP);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(coef2_start+i*2,streamFileGSP);
            }
        }
    }


	close_streamfile(streamFileGSP); streamFileGSP=NULL;

    return vgmstream;

    
    /* clean up anything we may have opened */
fail:
    if (streamFileGSP) close_streamfile(streamFileGSP);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
