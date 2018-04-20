#include "meta.h"


/* XA2 (RC Revenge Pro) */
VGMSTREAM * init_vgmstream_ps2_xa2_rrp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xa2",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x50,streamFile) != 0x00000000)
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x0,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x800;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    vgmstream->layout_type = layout_interleave;
    if (channel_count > 2)
        vgmstream->interleave_block_size = 0x400;
    else
    vgmstream->interleave_block_size = 0x1000;
    vgmstream->meta_type = meta_PS2_XA2_RRP;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
