#include "meta.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_mn_str(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;
	int bitspersample;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mnstr",filename_extension(filename))) goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x50,streamFile);
    bitspersample = read_32bitLE(0x58,streamFile);
	
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = read_32bitLE(0x20,streamFile)+0x48;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x54,streamFile);

	switch (bitspersample) {
		case 0x10:
			vgmstream->coding_type = coding_PCM16LE;
			if (channel_count == 1)
			{
				vgmstream->layout_type = layout_none;
			}
			else
			{
				vgmstream->interleave_block_size = 0x2;
				vgmstream->layout_type = layout_interleave;
			}
		break;
		case 0x4:
			if (read_32bitLE(0x20,streamFile) == 0x24)
			{
				vgmstream->interleave_block_size = 0x800;
				vgmstream->layout_type = layout_none;
			}
			break;
		default:
		    goto fail;
	}

    vgmstream->num_samples = read_32bitLE(0x4C,streamFile);

    //vgmstream->layout_type = layout_interleave;
    //vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_MN_STR;

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
