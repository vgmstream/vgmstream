#include "meta.h"
#include "../util.h"

/* DSP
	Teenage Mutant Ninja Turtles 2 (NGC)
*/
VGMSTREAM * init_vgmstream_ngc_dsp_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int loop_flag = 0;
	int channel_count;
    int i, j;
    off_t ch1_start;
    off_t ch2_start;
    off_t coef_table[2] = {0x90, 0xD0};

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
    vgmstream->meta_type = meta_NGC_DSP_KONAMI;

    ch1_start = 0x800;
    ch2_start = 0x800 + vgmstream->interleave_block_size;

    // COEFFS
		{
			for (j=0;j<vgmstream->channels;j++) {
				for (i=0;i<16;i++) {
					vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(coef_table[j]+i*2,streamFile);
				}
			}
		}

    /* open the file for reading */
    /* Channel 1 */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[0].streamfile)
    	goto fail;
    vgmstream->ch[0].channel_start_offset = vgmstream->ch[0].offset=ch1_start;
    
    /* Channel 1 */
    vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[1].streamfile)
    	goto fail;
    vgmstream->ch[1].channel_start_offset = vgmstream->ch[1].offset=ch2_start;

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
