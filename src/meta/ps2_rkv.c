#include "meta.h"
#include "../util.h"

/* RKV (from Legacy of Kain - Blood Omen 2) */
VGMSTREAM * init_vgmstream_ps2_rkv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset=0;
    int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rkv",filename_extension(filename))) goto fail;

	// Some RKV got info @ offset 0
	// Some other @ offset 4
	if(read_32bitLE(0,streamFile)==0) 
		start_offset=4;

	loop_flag = (read_32bitLE(start_offset+4,streamFile)!=0xFFFFFFFF);
    channel_count = read_32bitLE(start_offset+0x0c,streamFile)+1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
	vgmstream->sample_rate = read_32bitLE(start_offset,streamFile);
    vgmstream->coding_type = coding_PSX;

	// sometimes sample count is not set on the header 
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)/16*28/channel_count;
    
	if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(start_offset+4,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(start_offset+8,streamFile);
    }

	start_offset = 0x800;

	if((get_streamfile_size(streamFile)-0x800)%0x400) 
	{
		vgmstream->layout_type = layout_interleave_shortblock;
		vgmstream->interleave_smallblock_size=((get_streamfile_size(streamFile)-0x800)%0x400)/channel_count;
	} else {
		vgmstream->layout_type = layout_interleave;
	}

	vgmstream->interleave_block_size = 0x400;
	vgmstream->meta_type = meta_PS2_RKV;
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
