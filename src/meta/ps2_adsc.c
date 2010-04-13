#include "meta.h"
#include "../util.h"

/* ADSC (from Kenka Bancho 2: Full Throttle) */
VGMSTREAM * init_vgmstream_ps2_adsc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
   int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ads",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41445343) /* ADSC */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x18,streamFile);
    
   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x1000;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x14,streamFile);
    vgmstream->coding_type = coding_PSX;
	if(read_32bitLE(0x18,streamFile)==0x01) 
    vgmstream->num_samples = read_32bitLE(0x2c,streamFile)*56/32;
	else
    vgmstream->num_samples = read_32bitLE(0x2c,streamFile)*28/32;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x1c,streamFile);
    vgmstream->meta_type = meta_PS2_ADSC;

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
