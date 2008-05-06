#include <math.h>
#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_ngc_adpdtk(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;
    
    size_t file_size;
    int i;

    /* check extension, case insensitive */
    if (strcasecmp("adp",filename_extension(filename))) goto fail;

    /* try to open the file for checking */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* file size is the only way to determine sample count */
    file_size = get_streamfile_size(infile);

    /* .adp files have no header, so all we can do is look for a valid first frame */
    if (read_8bit(0,infile)!=read_8bit(2,infile) || read_8bit(1,infile)!=read_8bit(3,infile)) goto fail;

    /* done with checking */
    close_streamfile(infile);

    /* Hopefully we haven't falsely detected something else... */
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(2,0);    /* always stereo, no loop */
    if (!vgmstream) goto fail;

    vgmstream->num_samples = file_size/32*28;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_NGC_DTK;
    vgmstream->layout_type = layout_dtk_interleave;
    vgmstream->meta_type = meta_NGC_ADPDTK;

    for (i=0;i<2;i++) {
        vgmstream->ch[i].channel_start_offset =
            vgmstream->ch[i].offset = 0;

        vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,32*0x400);
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

