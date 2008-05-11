#include "meta.h"
#include "../util.h"

/* INT

   PS2 INT format is a RAW 48khz PCM file without header                
   The only fact about those file, is that the raw is interleaved 

   The interleave value is allways 0x200
   known extensions : INT

   2008-05-11 - Fastelbja : First version ...
*/

VGMSTREAM * init_vgmstream_ps2_int(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
	int i;

    /* check extension, case insensitive */
    if (strcasecmp("int",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* No check to do as they are raw pcm */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = 2;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = get_streamfile_size(infile)/4;
    vgmstream->interleave_block_size = 0x200;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_RAW;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x8000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
