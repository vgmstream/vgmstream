#include "meta.h"
#include "../util.h"

/* .SC - from Activision's EXAKT system, seen in Supercar Street Challenge */

VGMSTREAM * init_vgmstream_exakt_sc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    size_t file_size;

    /* check extension, case insensitive */
    /* this is all we have to go on, SC is completely headerless */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sc",filename_extension(filename))) goto fail;

    file_size = get_streamfile_size(streamFile);

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = file_size / 2;
    vgmstream->sample_rate = 48000;

    vgmstream->coding_type = coding_SASSC;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x100;
    vgmstream->meta_type = meta_EXAKT_SC;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

