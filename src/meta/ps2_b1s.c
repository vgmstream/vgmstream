#include "meta.h"
#include "../util.h"

/* B1S (found in 7 Wonders of the Ancient World) */
VGMSTREAM * init_vgmstream_ps2_b1s(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
		int channel_count;
		off_t start_offset;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("b1s",filename_extension(filename))) goto fail;

    if ((read_32bitLE(0x04,streamFile)+0x18) != get_streamfile_size(streamFile))
        goto fail;

    channel_count = read_32bitLE(0x14,streamFile);

		/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

		/* fill in the vital statistics */
    start_offset = 0x18;
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x04,streamFile)/16/channel_count*28;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x0C,streamFile);
    vgmstream->meta_type = meta_PS2_B1S;

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
