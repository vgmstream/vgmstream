#include "meta.h"
#include "bar_streamfile.h"

/* Guitar Hero III Mobile .bar */
VGMSTREAM * init_vgmstream_bar(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE* streamFileBAR = NULL; // don't close, this is just the source streamFile wrapped
    char filename[PATH_LIMIT];
    off_t start_offset;
    off_t ch2_start_offset;
    int loop_flag;
	int channel_count;
    long file_size;


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bar",filename_extension(filename))) goto fail;

    /* decryption wrapper for header reading */
    streamFileBAR = wrap_bar_STREAMFILE(streamFile);
    if (!streamFileBAR) goto fail;

    file_size = get_streamfile_size(streamFileBAR);

    /* check header */
    if (read_32bitBE(0x00,streamFileBAR) != 0x11000100 ||
        read_32bitBE(0x04,streamFileBAR) != 0x01000200) goto fail;
    if (read_32bitLE(0x50,streamFileBAR) != file_size) goto fail;

    start_offset = read_32bitLE(0x18,streamFileBAR);
    if (0x54 != start_offset) goto fail;
    ch2_start_offset = read_32bitLE(0x48,streamFileBAR);
    if (ch2_start_offset >= file_size) goto fail;

    /* build the VGMSTREAM */
    channel_count = 2;
    loop_flag = 0;
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 11025;
    vgmstream->coding_type = coding_IMA;
    vgmstream->num_samples = (file_size-ch2_start_offset)*2;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_GH3_BAR;

    {
        STREAMFILE *file1, *file2;
        file1 = streamFileBAR->open(streamFileBAR,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file1) goto fail;
        file2 = streamFileBAR->open(streamFileBAR,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file2)
        {
            close_streamfile(file1);
            goto fail;
        }
        vgmstream->ch[0].streamfile = file1;
        vgmstream->ch[1].streamfile = file2;
        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=start_offset;
        vgmstream->ch[1].channel_start_offset=
            vgmstream->ch[1].offset=ch2_start_offset;
    }

    // discard our decrypt wrapper, without closing the original streamfile
    free(streamFileBAR);

    return vgmstream;
fail:
    if (streamFileBAR)
        free(streamFileBAR);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
