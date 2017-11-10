#include "meta.h"
#include "../util.h"

/* GUN (Gunvari Streams) */
VGMSTREAM * init_vgmstream_ps2_mcg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mcg",filename_extension(filename))) goto fail;

    /* check header */
    if (!((read_32bitBE(0x00,streamFile) == 0x4D434700) && 
         (read_32bitBE(0x20,streamFile) == 0x56414770) && 
         (read_32bitBE(0x50,streamFile) == 0x56414770)))
        goto fail;

    loop_flag = (read_32bitLE(0x34,streamFile)!=0);
    channel_count = 2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x80;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x30,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitBE(0x2C,streamFile)/16*14*channel_count;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);
    vgmstream->meta_type = meta_PS2_MCG;

    if (vgmstream->loop_flag)
    {
        vgmstream->loop_start_sample = read_32bitLE(0x34,streamFile);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

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
