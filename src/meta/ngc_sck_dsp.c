#include "meta.h"
#include "../util.h"

/* 
    SCK+DSP
    2009-08-25 - manakoAT : Scorpion King (NGC)...
*/

VGMSTREAM * init_vgmstream_ngc_sck_dsp(STREAMFILE *streamFile) {

	VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamFileDSP = NULL;
    char filename[260];
	char filenameDSP[260];
	
	int i;
	int channel_count;
	int loop_flag;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sck",filename_extension(filename))) goto fail;


	strcpy(filenameDSP,filename);
	strcpy(filenameDSP+strlen(filenameDSP)-3,"dsp");

	streamFileDSP = streamFile->open(streamFile,filenameDSP,STREAMFILE_DEFAULT_BUFFER_SIZE);
	
    if (read_32bitBE(0x5C,streamFile) != 0x60A94000)
        goto fail;

	if (!streamFile) goto fail;
	
	channel_count = 2;
	loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->channels = channel_count;
	vgmstream->sample_rate = read_32bitBE(0x18,streamFile);
	vgmstream->num_samples=read_32bitBE(0x14,streamFile)/8/channel_count*14;
	vgmstream->coding_type = coding_NGC_DSP;
	
	if(loop_flag) {
		vgmstream->loop_start_sample = 0;
		vgmstream->loop_end_sample = read_32bitBE(0x10,streamFile)/8/channel_count*14;
	}	

	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count == 2) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size=read_32bitBE(0xC,streamFile);
	}



    vgmstream->meta_type = meta_NGC_SCK_DSP;
	
    /* open the file for reading */
    {
        for (i=0;i<channel_count;i++) {
			/* Not sure, i'll put a fake value here for now */
            vgmstream->ch[i].streamfile = streamFile->open(streamFileDSP,filenameDSP,0x8000);
            vgmstream->ch[i].offset = 0;

            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }


    
	if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x2C+i*2,streamFile);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x2C+i*2,streamFile);
            }
        }
    }


	close_streamfile(streamFileDSP); streamFileDSP=NULL;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (streamFileDSP) close_streamfile(streamFileDSP);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
