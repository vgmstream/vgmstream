#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* The standard .dsp */

VGMSTREAM * init_vgmstream_ngc_dsp_std(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag;
    off_t start_offset;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("dsp",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    start_offset = 0x60;

    /* check initial predictor/scale */
    if (read_16bitBE(0x3e,infile) != (uint8_t)read_8bit(start_offset,infile))
        goto fail;

    /* check type==0 and gain==0 */
    if (read_16bitBE(0x0e,infile) || read_16bitBE(0x3c,infile))
        goto fail;
        
    loop_flag = read_16bitBE(0xc,infile);
    if (loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = read_32bitBE(0x10,infile)/16*8;
        if (read_16bitBE(0x44,infile) != (uint8_t)read_8bit(start_offset+loop_off,infile))
            goto fail;
    }

    /* compare num_samples with nibble count */
    /*
    fprintf(stderr,"num samples (literal): %d\n",read_32bitBE(0,infile));
    fprintf(stderr,"num samples (nibbles): %d\n",dsp_nibbles_to_samples(read_32bitBE(4,infile)));
    */

    /* build the VGMSTREAM */


    vgmstream = allocate_vgmstream(1,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0,infile);
    vgmstream->sample_rate = read_32bitBE(8,infile);

    /*
    vgmstream->loop_start_sample =
            read_32bitBE(0x10,infile)/16*14;
    vgmstream->loop_end_sample = 
            read_32bitBE(0x14,infile)/16*14;
            */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(
            read_32bitBE(0x10,infile));
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            read_32bitBE(0x14,infile))+1;
    /* don't know why, but it does happen*/
    if (vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    start_offset = 0x60;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DSP_STD;

    for (i=0;i<16;i++)
        vgmstream->ch[0].adpcm_coef[i]=read_16bitBE(0x1c+i*2,infile);

    close_streamfile(infile); infile=NULL;

    /* open the file for reading */
    {
        vgmstream->ch[0].streamfile = open_streamfile(filename);

        if (!vgmstream->ch[0].streamfile) goto fail;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
