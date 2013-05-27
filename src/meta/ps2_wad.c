#include "meta.h"
#include "../util.h"

/* WAD (from The golden Compass) */
VGMSTREAM * init_vgmstream_ps2_wad(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int loop_flag = 0;
		int channel_count;
		off_t start_offset;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wad",filename_extension(filename))) goto fail;

    /* check header */
    if ((read_32bitLE(0x00,streamFile)+0x40) != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = 0;
    channel_count = (uint16_t) read_16bitLE(0x4,streamFile);
    
		/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

		/* fill in the vital statistics */
    start_offset = 0x40;
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = (uint16_t) read_16bitLE(0x6,streamFile);;
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x0,streamFile)/channel_count/16*28;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x0,streamFile)/channel_count/16*28;
    }

		if (channel_count == 1)
		{
			vgmstream->layout_type = layout_none;
		}
		else
		{
			goto fail;
		}

    vgmstream->meta_type = meta_PS2_WAD;

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
