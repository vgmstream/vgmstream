#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_stx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    const int loop_flag = 0;
    const int channel_count = 2;    /* .stx seems to be stereo only */

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("stx",filename_extension(filename))) goto fail;

    /* length of data */
    if (read_32bitBE(0x00,streamFile) !=
        get_streamfile_size(streamFile) - 0x20) goto fail;

    /* bits per sample? */
    if (read_16bitBE(0x0a,streamFile) != 4) goto fail;

    /* samples per frame? */
    if (read_16bitBE(0x0c,streamFile) != 0x10) goto fail;

    /* ?? */
    if (read_16bitBE(0x0e,streamFile) != 0x1E) goto fail;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;


    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0x04,streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitBE(0x08,streamFile);
    /* channels and loop flag are set by allocate_vgmstream */

    vgmstream->coding_type = coding_NGC_AFC;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_STX;

    /* frame-level interleave (9 bytes) */
    vgmstream->interleave_block_size = 9;

    /* open the file for reading by each channel */
    {
        STREAMFILE *chstreamfile;
        int i;

        /* both channels use same buffer, as interleave is so small */
        chstreamfile = streamFile->open(streamFile,filename,9*channel_count*0x100);
        if (!chstreamfile) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = chstreamfile;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                0x20 + i*vgmstream->interleave_block_size;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
