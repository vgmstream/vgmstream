#include "meta.h"
#include "../util.h"

/* .RXW - presumably fake header for split files with joint XWH+XWB (has incorrect chunk sizes) */
VGMSTREAM * init_vgmstream_ps2_rxw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag=0, channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"rxw")) goto fail;

    /* check RXWS/FORM Header */
    if (!((read_32bitBE(0x00,streamFile) == 0x52585753) && 
	      (read_32bitBE(0x10,streamFile) == 0x464F524D)))
        goto fail;

	loop_flag = (read_32bitLE(0x3C,streamFile)!=0xFFFFFFFF);
	channel_count=2; /* Always stereo files */
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x2E,streamFile);
    vgmstream->num_samples = (read_32bitLE(0x38,streamFile)*28/16)/2;

	/* Get loop point values */
	if(vgmstream->loop_flag) {
		vgmstream->loop_start_sample = read_32bitLE(0x3C,streamFile)/16*14;
		vgmstream->loop_end_sample = read_32bitLE(0x38,streamFile)/16*14;
	}

    vgmstream->interleave_block_size = read_32bitLE(0x1c,streamFile)+0x10;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_RXWS;
	start_offset = 0x40;

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
