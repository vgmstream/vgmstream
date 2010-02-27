#include "meta.h"
#include "../util.h"

/* AAAP
	found in: Turok Evoluttion (NGC)
*/
VGMSTREAM * init_vgmstream_ngc_aaap(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
		int channel_count;
		int i, j;
		off_t start_offset;
		off_t coef_table[8] = {0x24,0x84,0xE4,0x144,0x1A4,0x204,0x264,0x2C4};

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aaap",filename_extension(filename)))
				goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41414170) /* "AAAp" */
        goto fail;

    loop_flag = 0;
    channel_count = (uint16_t)read_16bitBE(0x06,streamFile);
    
    if ((read_32bitBE(0x0C,streamFile)+0x8+(channel_count*0x60)) != (get_streamfile_size(streamFile)))
        goto fail;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x8 + (channel_count * 0x60);
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x8,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitBE(0x8,streamFile);
    }

		if (channel_count == 1) {
				vgmstream->layout_type = layout_none;
		} else if (channel_count > 1) {
    		vgmstream->layout_type = layout_interleave;
				vgmstream->interleave_block_size = (uint16_t)read_16bitBE(0x04,streamFile);
    }
    	
    	vgmstream->meta_type = meta_NGC_AAAP;

		{
			for (j=0;j<vgmstream->channels;j++) {
				for (i=0;i<16;i++) {
					vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
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
