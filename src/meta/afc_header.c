#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_afc(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag;
    const int channel_count = 2;    /* .afc seems to be stereo only */

    /* check extension, case insensitive */
    if (strcasecmp("afc",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* we will get a sample rate, that's as close to checking as I think
     * we can get */

    /* build the VGMSTREAM */

    loop_flag = read_32bitBE(0x10,infile);

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0x04,infile);
    vgmstream->sample_rate = (uint16_t)read_16bitBE(0x08,infile);
    /* channels and loop flag are set by allocate_vgmstream */
    vgmstream->loop_start_sample = read_32bitBE(0x14,infile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_AFC;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_AFC;

    /* frame-level interleave (9 bytes) */
    vgmstream->interleave_block_size = 9;

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,9*0x400);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                0x20 + i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
