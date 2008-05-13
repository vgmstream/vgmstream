#include "meta.h"
#include "../util.h"

/* EXST

   PS2 INT format is an interleaved format found in Shadow of the Colossus                
   The header start with a EXST id.
   The headers and bgm datas was separated in the game, and joined in order
   to add support for vgmstream

   The interleave value is allways 0x400
   known extensions : .STS

   2008-05-13 - Fastelbja : First version ...
*/

VGMSTREAM * init_vgmstream_ps2_exst(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag=0;
    int channel_count;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("sts",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check EXST Header */
    if (read_32bitBE(0x00,infile) != 0x45585354)
        goto fail;

    /* check loop */
    loop_flag = (read_32bitLE(0x0C,infile)==1);

    channel_count=read_16bitLE(0x06,infile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = read_16bitLE(0x06,infile);
    vgmstream->sample_rate = read_32bitLE(0x08,infile);

    /* Compression Scheme */
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (read_32bitLE(0x14,infile)*0x400)/16*28;

    /* Get loop point values */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = (read_32bitLE(0x10,infile)*0x400)/16*28;
        vgmstream->loop_end_sample = (read_32bitLE(0x14,infile)*0x400)/16*28;
    }

    vgmstream->interleave_block_size = 0x400;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_EXST;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x8000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                0x800+vgmstream->interleave_block_size*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
