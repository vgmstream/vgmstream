#include "meta.h"
#include "../util.h"

/* .agsc - from Metroid Prime 2 */

VGMSTREAM * init_vgmstream_agsc(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    off_t header_offset;
    off_t start_offset;
    int channel_count;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("agsc",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,infile)!=0x00000001)
        goto fail;

    /* count length of name, including terminating 0 */
    for (header_offset=4;header_offset < get_streamfile_size(infile) && read_8bit(header_offset,infile)!='\0';header_offset++);

    header_offset ++;

    channel_count = 1;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(1,1);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(header_offset+0xda,infile);
    vgmstream->sample_rate = (uint16_t)read_16bitBE(header_offset+0xd8,infile);

    vgmstream->loop_start_sample = read_32bitBE(header_offset+0xde,infile);
    /* this is cute, we actually have a "loop length" */
    vgmstream->loop_end_sample = (vgmstream->loop_start_sample + read_32bitBE(header_offset+0xe2,infile))-1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DSP_AGSC;

    for (i=0;i<16;i++) {
        vgmstream->ch[0].adpcm_coef[i]=read_16bitBE(header_offset+0xf6+i*2,infile);
    }

    start_offset = header_offset+0x116;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile(filename);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                start_offset;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
