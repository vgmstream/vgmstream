#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* IAB: Ueki no Housoku - Taosu ze Robert Juudan!! (PS2) */
VGMSTREAM * init_vgmstream_ps2_iab(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
	int channel_count;
    int i;
	off_t start_offset;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("iab",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x10000000)
        goto fail;
    
    /* check file size */
    if (read_32bitLE(0x1C,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x40;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x4,streamFile);
    vgmstream->coding_type = coding_PSX;

    vgmstream->layout_type = layout_ps2_iab_blocked;
    vgmstream->interleave_block_size = read_32bitLE(0xC, streamFile);
    vgmstream->meta_type = meta_PS2_IAB;
    
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) 
		{
            vgmstream->ch[i].streamfile = streamFile->open(streamFile, filename, vgmstream->interleave_block_size);            
			if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    /* Calc num_samples */
    ps2_iab_block_update(start_offset, vgmstream);
    vgmstream->num_samples=0;

    do 
	{    
		vgmstream->num_samples += 0x4000 * 14 / 16;
        ps2_iab_block_update(vgmstream->next_block_offset, vgmstream);
    } while (vgmstream->next_block_offset < get_streamfile_size(streamFile));

    ps2_iab_block_update(start_offset, vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
