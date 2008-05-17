#include "meta.h"
#include "../util.h"

/* RAW

   RAW format is native 44khz PCM file
   Nothing more :P ...

   2008-05-17 - Fastelbja : First version ...
*/

VGMSTREAM * init_vgmstream_raw(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
	int i;

    /* check extension, case insensitive */
    if (strcasecmp("raw",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* No check to do as they are raw pcm */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = 2;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (int32_t)(get_streamfile_size(infile)/2);
    vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = 2;
    vgmstream->meta_type = meta_RAW;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x1000);

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
