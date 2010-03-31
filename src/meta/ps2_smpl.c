#include "meta.h"
#include "../util.h"

/* SMPL (from Homura) */
VGMSTREAM * init_vgmstream_ps2_smpl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("smpl",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x534D504C) /* "SMPL" */
        goto fail;

    loop_flag = 1;
    channel_count = 1;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x40;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX_badflags;
    vgmstream->num_samples = read_32bitBE(0xc,streamFile)*56/32;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x30,streamFile);
       vgmstream->loop_end_sample = vgmstream->num_samples;
    }
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_PS2_SMPL;

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
