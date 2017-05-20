#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* STR: The Bouncer (PS2) */
VGMSTREAM * init_vgmstream_ps2_strlr(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int loop_flag = 0;
	int channel_count;
    int i;
	off_t start_offset;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("str",filename_extension(filename))) goto fail;

#if 0
    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5354524C) /* "STRL" */
        goto fail;
	if (read_32bitBE(0x00,streamFile) != 0x53545252) /* "STRR" */
        goto fail;
#endif

	/* don't hijack Sonic & Sega All Stars Racing X360 (xma) */
	if (read_32bitBE(0x00,streamFile) == 0x52494646) /* "RIFF"*/
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x0;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_PSX;

    vgmstream->layout_type = layout_ps2_strlr_blocked;
    //vgmstream->interleave_block_size = read_32bitLE(0xC, streamFile);
    vgmstream->meta_type = meta_PS2_STRLR;
    
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) 
		{
            vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename, 0x8000);            
			if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    /* Calc num_samples */
    ps2_strlr_block_update(start_offset, vgmstream);
    vgmstream->num_samples=0;

    do
	{
		vgmstream->num_samples += vgmstream->current_block_size * 14 / 16;
        ps2_strlr_block_update(vgmstream->next_block_offset, vgmstream);
    } while (vgmstream->next_block_offset < get_streamfile_size(streamFile));

    ps2_strlr_block_update(start_offset, vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
