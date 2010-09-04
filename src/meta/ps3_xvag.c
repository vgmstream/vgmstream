#include "meta.h"
#include "../util.h"

/* XVAG (from Ratchet & Clank Future: Quest for Booty) */
VGMSTREAM * init_vgmstream_ps3_xvag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xvag",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
        goto fail;

    if (read_32bitBE(0x44,streamFile) !=0x63756573)
	loop_flag = 0;
	else loop_flag = 1;

    channel_count = read_32bitBE(0x28,streamFile);

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = read_32bitBE(0x4,streamFile);
   vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x3c,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitBE(0x30,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x24,streamFile);;
       vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS3_XVAG;

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
