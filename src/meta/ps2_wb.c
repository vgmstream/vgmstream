#include "meta.h"
#include "../util.h"

/* WB (from Shooting Love. ~TRIZEAL~) */
VGMSTREAM * init_vgmstream_ps2_wb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

    int loop_flag;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0,streamFile) != 0x00000000)
        goto fail;
	    
    loop_flag = read_32bitLE(0x4,streamFile);
    channel_count = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x10;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = read_32bitLE(0xC,streamFile)/4;
    if (loop_flag) {
       vgmstream->loop_start_sample = read_32bitLE(0x4,streamFile);
       vgmstream->loop_end_sample = read_32bitLE(0x8,streamFile);
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 2;
    vgmstream->meta_type = meta_PS2_WB;
   
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
