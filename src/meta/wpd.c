#include "meta.h"
#include "../util.h"

/* WPD (from Shuffle! (PC)) */
VGMSTREAM * init_vgmstream_wpd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int channel_count;
    int loop_flag;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wpd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x20445057) /* " DPW" */
        goto fail;

    channel_count = read_32bitLE(0x4,streamFile);
    loop_flag = 0;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x30;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (read_32bitLE(0x14,streamFile))/2/channel_count;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 2;
    vgmstream->meta_type = meta_WPD;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+
                vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
