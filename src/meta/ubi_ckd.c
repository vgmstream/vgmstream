#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"

/* Ubisoft CKD (Rayman Origins - Wii) */
VGMSTREAM * init_vgmstream_ubi_ckd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ckd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52494646) /* RIFF */
		goto fail;
	if (read_32bitBE(0x26,streamFile) != 0x6473704C) /* dspL */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitBE(0x16,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	if (read_16bitBE(0x16,streamFile) == 1) {
		start_offset = 0x96;
        vgmstream->num_samples = (read_32bitBE(0x92,streamFile))*28/16/channel_count;
	}
	else {
		start_offset = 0xFE;
	    vgmstream->num_samples = (read_32bitBE(0xFA,streamFile))*28/16/channel_count;
	}
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x18,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (read_32bitBE(0xFA,streamFile))*28/16/channel_count;

    vgmstream->layout_type = layout_interleave; 
    vgmstream->interleave_block_size = 8;
    vgmstream->meta_type = meta_UBI_CKD;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x4A+i*2,streamFile);
        }
        if (vgmstream->channels) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0xB2+i*2,streamFile);
            }
        }
    }
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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

