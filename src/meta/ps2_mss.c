#include "meta.h"
#include "../util.h"

/**
 * Guerrilla's MSS
 *
 * Found in ShellShock Nam '67, Killzone (PS2)
 */
VGMSTREAM * init_vgmstream_ps2_mss(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	off_t start_offset;
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mss",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D435353) /* "MCSS" */
        goto fail;

    loop_flag = 0;
    channel_count = read_16bitLE(0x16,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = read_32bitLE(0x08,streamFile);
	vgmstream->channels = channel_count;
	/*datasize = read_32bitLE(0x0c,streamFile) */
	vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
	vgmstream->num_samples = read_32bitLE(0x1C,streamFile);/*  / 16 * 28 */
    vgmstream->coding_type = coding_PSX;


	if (channel_count == 1)
	{
		vgmstream->layout_type = layout_none;
	}
	else
	{
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = read_32bitLE(0x18,streamFile);
	}

    vgmstream->meta_type = meta_PS2_MSS;

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
