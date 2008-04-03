#include "gcsw.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_gcsw(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int channel_count;
    int loop_flag;

    /* check extension, case insensitive */
    if (strcasecmp("gcw",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

   /* check header */
    if ((uint32_t)read_32bitBE(0,infile)!=0x47435357) /* "GCSW" */
        goto fail;

    /* check type details */
    /* guess */
    loop_flag = read_32bitBE(0x1c,infile);
    channel_count = read_32bitBE(0xc,infile);

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0x10,infile);
    vgmstream->sample_rate = read_32bitBE(0x8,infile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = read_32bitBE(0x14,infile);
    vgmstream->loop_end_sample = read_32bitBE(0x18,infile);

    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_GCSW;

    vgmstream->interleave_block_size = 0x8000;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,0x8000
                    );

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                0x20+0x8000*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
