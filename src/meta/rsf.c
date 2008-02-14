#include "rsf.h"
#include "../util.h"
#include "../coding/g721_decoder.h"

/* .rsf - from Metroid Prime */

VGMSTREAM * init_vgmstream_rsf(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int i;
    size_t file_size;

    /* check extension, case insensitive */
    /* this is all we have to go on, rsf is completely headerless */
    if (strcasecmp("rsf",filename_extension(filename))) goto fail;

    /* try to open the file so we can count the filesize */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    file_size = get_streamfile_size(infile);

    close_streamfile(infile);
    infile = NULL;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = file_size/2*2;
    vgmstream->sample_rate = 32000;

    vgmstream->coding_type = coding_G721;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSF;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = open_streamfile(filename);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                file_size/2*i;


            g72x_init_state(&(vgmstream->ch[i].g72x_state));
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
