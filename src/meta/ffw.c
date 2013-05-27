#include "meta.h"
#include "../util.h"

/* FFW (from Freedom Fighters [NGC]) */
VGMSTREAM * init_vgmstream_ffw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
		int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ffw",filename_extension(filename))) goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x11C,streamFile);
    
		/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

		/* fill in the vital statistics */
    start_offset = 0x130;
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10C,streamFile);
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;

    if (channel_count == 1)
    {
        vgmstream->layout_type = layout_none;
    }
    else
    {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x10000;
    }
    
    vgmstream->meta_type = meta_FFW;

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
