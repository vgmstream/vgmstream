#include "meta.h"
#include "../util.h"

/* SVAG

   PS2 SVAG format is an interleaved format found in many konami Games                
   The header start with a Svag id and have the sentence :
		"ALL RIGHTS RESERVED.KONAMITYO Sound Design Dept. "
	or  "ALL RIGHTS RESERVED.KCE-Tokyo Sound Design Dept. "

   2008-05-13 - Fastelbja : First version ...
							Thx to HCS for his awesome work on shortblock interleave
*/

VGMSTREAM * init_vgmstream_ps2_svag(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag=0;
    int channel_count;
    int i;
    off_t start_offset;

    /* check extension, case insensitive */
    if (strcasecmp("svag",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check SVAG Header */
    if (read_32bitBE(0x00,infile) != 0x53766167)
        goto fail;

    /* check loop */
    loop_flag = (read_32bitLE(0x14,infile)==1);

    channel_count=read_16bitLE(0x0C,infile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = read_16bitLE(0x0C,infile);
    vgmstream->sample_rate = read_32bitLE(0x08,infile);

    /* Compression Scheme */
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x04,infile)/16*28/vgmstream->channels;

    /* Get loop point values */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x18,infile)/16*28;
        vgmstream->loop_end_sample = read_32bitLE(0x04,infile)/16*28/vgmstream->channels;
    }

    vgmstream->interleave_block_size = read_32bitLE(0x10,infile);
	vgmstream->interleave_smallblock_size = (read_32bitLE(0x04,infile)%(2*vgmstream->interleave_block_size))/2;
    vgmstream->layout_type = layout_interleave_shortblock;
    vgmstream->meta_type = meta_PS2_SVAG;

    /* 2008-05-16 - hcs - use Fastelbja's check from in_cube to decide the start offset */
    if ((uint32_t)read_32bitBE(0x1c,infile)==0x20414c4c) /* " ALL" */
        start_offset = 0x800;
    else {
        /* this form includes the header in the value at 0x04 */
        start_offset = 0x400;
        vgmstream->interleave_smallblock_size = ((read_32bitLE(0x04,infile)-0x400)%(2*vgmstream->interleave_block_size))/2;
        vgmstream->num_samples = (read_32bitLE(0x04,infile)-0x400)/16*28/vgmstream->channels;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,vgmstream->interleave_block_size);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                start_offset+(off_t)(vgmstream->interleave_block_size*i);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
