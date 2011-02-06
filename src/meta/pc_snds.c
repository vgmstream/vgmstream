#include "meta.h"
#include "../util.h"

/* .snds - from Incredibles PC */

VGMSTREAM * init_vgmstream_pc_snds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    size_t file_size;
    int i;

    /* check extension, case insensitive */
    /* this is all we have to go on, snds is completely headerless */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("snds",filename_extension(filename))) goto fail;

    file_size = get_streamfile_size(streamFile);

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 48000;

    /* file seems to be mistakenly 1/8 too long */
    vgmstream->num_samples = file_size*8/9;

    /* check for 32 0 bytes where the padding should start */
    for (i = 0; i < 8; i++)
    {
        if (read_32bitBE(vgmstream->num_samples+i*4,streamFile) != 0)
        {
            /* not padding? just play the whole file */
            vgmstream->num_samples = file_size;
            break;
        }
    }

    vgmstream->coding_type = coding_SNDS_IMA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_PC_SNDS;

    /* open the file for reading */
    vgmstream->ch[0].streamfile = vgmstream->ch[1].streamfile =
        streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

    if (!vgmstream->ch[0].streamfile) goto fail;

    vgmstream->ch[0].channel_start_offset=
        vgmstream->ch[0].offset=
        vgmstream->ch[1].channel_start_offset=
        vgmstream->ch[1].offset=0;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

