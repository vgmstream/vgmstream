#include "meta.h"
#include "../util.h"

/* DSP
	Teenage Mutant Ninja Turtles 2 (NGC)
*/
VGMSTREAM * init_vgmstream_ngc_dsp_tmnt2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
		int channel_count;
		int i, j;
		off_t start_offset;
		off_t coef_table[8] = {0x90,0xD0,0x110,0x150,0x190,0x1D0,0x210,0x250};

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("dsp",filename_extension(filename)))
				goto fail;

    /* check header */
    if ((read_32bitBE(0x00,streamFile)+0x800) != (get_streamfile_size(streamFile)))
				goto fail;

    loop_flag = (read_32bitBE(0x10,streamFile) != 0x0);
    channel_count = 2;

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = 0x800;
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x04,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (read_32bitBE(0x00,streamFile)/channel_count/8*14);
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0x14,streamFile)/channel_count/8*14);
        vgmstream->loop_end_sample = (read_32bitBE(0x00,streamFile)/channel_count/8*14);
    }

  		  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x100;
    	  vgmstream->meta_type = meta_NGC_DSP_TMNT2;

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
