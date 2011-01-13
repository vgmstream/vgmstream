#include "meta.h"
#include "../util.h"

/* .lsf - Fastlane Street Racing (iPhone) */
/* "!n1nj4n" */

VGMSTREAM * init_vgmstream_lsf_n1nj4n(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    size_t file_size;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("lsf",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0, streamFile) != 0x216E316E || // "!n1n"
        read_32bitBE(0x4, streamFile) != 0x6A346E00)   // "j4n\0"
        goto fail;

    /* check size */
    file_size = get_streamfile_size(streamFile);
    if (read_32bitLE(0xC, streamFile) + 0x10 != file_size)
        goto fail;

    start_offset = 0x10;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(1,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = (file_size-0x10)/0x1c*0x1b*2;
    vgmstream->sample_rate = read_32bitLE(0x8, streamFile);

    vgmstream->coding_type = coding_LSF;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_LSF_N1NJ4N;

    /* open the file for reading */
    {
        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

        if (!vgmstream->ch[0].streamfile) goto fail;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

