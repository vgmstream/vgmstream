#include "meta.h"
#include "../util.h"

/* VOI - found in "RAW Danger" (PS2) */
VGMSTREAM * init_vgmstream_ps2_voi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int loop_flag = 0;
		int channel_count;
    off_t start_offset;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("voi",filename_extension(filename))) goto fail;

    /* check header */
    if (((read_32bitLE(0x04,streamFile)*2)+0x800) != (get_streamfile_size(streamFile)))
    {
      goto fail;
    }

    loop_flag = 0;
    channel_count = read_32bitLE(0x00,streamFile);
    
	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = 0x800;
		vgmstream->channels = channel_count;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/channel_count/2;

    if (loop_flag)
    {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitLE(0x04,streamFile)/2);
    }

    if (read_32bitLE(0x08,streamFile) == 0)
    {
        vgmstream->sample_rate = 48000;
        vgmstream->interleave_block_size = 0x200;
    }
    else if (read_32bitLE(0x08,streamFile) == 1)
    {
        vgmstream->sample_rate = 24000;
        vgmstream->interleave_block_size = 0x100;
    }
    else
    {
      goto fail;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_VOI;

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
