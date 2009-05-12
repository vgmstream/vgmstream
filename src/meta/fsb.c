#include "meta.h"
#include "../util.h"

/* comment from hcs:
((uint8_t)read_8bit(offset, file))&0xf for the low nibble, 
((uint8_t)read_8bit(offset, file)) >> 4 for the high one
((uint8_t)read_8bit(0x4B,streamFile) >> (1?0:4))&0xf;
*/
/* FSB1 */
VGMSTREAM * init_vgmstream_fsb1(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	/* int fsb1_included_files; */
	int fsb1_format;
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534231) /* "FSB1" */
        goto fail;
 
	/* "Check if the FSB is used as
	conatiner or as single file" */
	if (read_32bitBE(0x04,streamFile) != 0x01000000)
		goto fail;

	loop_flag = 0;
	channel_count = 2;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* This will be tricky ;o) */
	fsb1_format = read_32bitBE(0x44,streamFile);
	switch (fsb1_format) {
		case 0x40008800: /* PS2 (Operation Genesis) */
		case 0x41008800: /* PS2 (Operation Genesis) */
		vgmstream->coding_type = coding_PSX;
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x10;
		vgmstream->num_samples = (read_32bitLE(0x34,streamFile))*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x30,streamFile);
    }
	break;
		default:
			goto fail;

	}
	/* fill in the vital statistics */
    start_offset = 0x50;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x38,streamFile);
    vgmstream->meta_type = meta_FSB1;

    
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



/* FSB3 */
VGMSTREAM * init_vgmstream_fsb3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	/* int fsb3_included_files; */
	int fsb3_headerlen = 0x18;
	int fsb3_format;
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534233) /* "FSB3" */
        goto fail;
 
	/* "Check if the FSB is used as
	conatiner or as single file" */
	if (read_32bitBE(0x04,streamFile) != 0x01000000)
		goto fail;
    


	if (read_32bitBE(0x48,streamFile) == 0x02000806) {
        loop_flag = 1;
    } else {
        loop_flag = 0; /* (read_32bitLE(0x08,streamFile)!=0); */
    }
    
	/* Channel check
	if (read_16bitLE(0x56,streamFile) == 2) {
        channel_count = 2;
    } else {
        goto fail;
    }
	*/

	channel_count = read_16bitLE(0x56,streamFile);


	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* This will be tricky ;o) */
	fsb3_format = read_32bitBE(0x48,streamFile);
	switch (fsb3_format) {
		case 0x40008800: /* PS2 (Agent Hugo, Flat Out 2) */
		case 0x41008800: /* PS2 (Flat Out) */
		case 0x42008800: /* PS2 (Jackass - The Game) */
		case 0x01008804: /* PS2 (Cold Fear) */
		case 0x02008804: /* PS2 (Shrek - Smash 'n Crash */
		vgmstream->coding_type = coding_PSX;
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x10;
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x40,streamFile);
        vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
    }
	break;
		case 0x00000806: /* WII (de Blob) */
		case 0x00000886: /* WII (de Blob) */
		case 0x01000806: /* WII (Metroid Prime 3) */
		case 0x02000806: /* WII (Metroid Prime 3) */
        case 0x20100002: /* WII (de Blob) */
        case 0x21100002: /* NGC (The Incredibles: Rise of the Underminer */
		case 0x40000802: /* WII (WWE Smackdown Vs. Raw 2008) */
		case 0x40000882: /* WII (Bully) */
		case 0x41000802: /* NGC (Dysney's Incredibles, The) */

		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->layout_type = layout_interleave_byte;
        vgmstream->interleave_block_size = 2;
		vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*14/8/channel_count;
	if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x40,streamFile);
        vgmstream->loop_end_sample = (read_32bitLE(0x0C,streamFile))*14/8/channel_count;
    }
	break;
		case 0x40004020: /* WII (Guitar Hero III), uses Xbox-ish IMA */
		case 0x400040A0: /* WII (Guitar Hero III), uses Xbox-ish IMA */
		case 0x41004800: /* XBOX (FlatOut, Rainbow Six - Lockdown) */
		case 0x01004804: /* XBOX (Cold Fear) <- maybe IMA??? */
		vgmstream->coding_type = coding_XBOX;
		vgmstream->layout_type = layout_none;
		vgmstream->num_samples = read_32bitLE(0x0C,streamFile)*64/36/channel_count;
	if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x40,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(0x0C,streamFile)*64/36/channel_count;
    }
	break;
		default:
			goto fail;
	}
	/* fill in the vital statistics */
    start_offset = (read_32bitLE(0x08,streamFile))+fsb3_headerlen;
    vgmstream->sample_rate = read_32bitLE(0x4C,streamFile);
    vgmstream->meta_type = meta_FSB3;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i,c;
        for (c=0;c<channel_count;c++) {
            for (i=0;i<16;i++) {
                vgmstream->ch[c].adpcm_coef[i] =
                    read_16bitBE(0x68+c*0x2e +i*2,streamFile);
            }
        }
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            
            if (vgmstream->coding_type == coding_XBOX) {
                /* xbox interleaving is a little odd */
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    start_offset+vgmstream->interleave_block_size*i;
            }
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/* FSB4 */
VGMSTREAM * init_vgmstream_fsb4(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	/* int fsb1_included_files; */
	int fsb4_format;
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534234) /* "FSB4" */
        goto fail;
 
	/* "Check if the FSB is used as
	conatiner or as single file" */
	if (read_32bitBE(0x04,streamFile) != 0x01000000)
		goto fail;
	
	
	if (read_32bitBE(0x60,streamFile) == 0x40008800 ||
        read_32bitBE(0x60,streamFile) == 0x40000802 ||
        read_32bitBE(0x60,streamFile) == 0x40100802) {
		loop_flag = 1;
	} else {
		loop_flag = 0;
		}

    if (read_32bitBE(0x60,streamFile) != 0x20000882 &&
        read_32bitBE(0x60,streamFile) != 0x20100002 &&
        read_32bitBE(0x60,streamFile) != 0x20100802 &&
        read_32bitBE(0x60,streamFile) != 0x20100082 &&
        read_32bitBE(0x60,streamFile) != 0x20000802) {
        channel_count = 2;
    } else {
        channel_count = 1;
    }

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
	
	
	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x64,streamFile);
	fsb4_format = read_32bitBE(0x60,streamFile);
	switch (fsb4_format) {
		/* PS2 (Spider Man - Web of Shadows), Speed Racer */		
		case 0x40008800:
		vgmstream->coding_type = coding_PSX;
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x10;
		vgmstream->num_samples = (read_32bitLE(0x54,streamFile))*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x50,streamFile);
    }
	break;
    /* WII (de Blob, Night at the Museum) */
        case 0x40000802:
        case 0x40000882:
        case 0x40100802:
    if (read_32bitLE(0x14,streamFile)==0x20)
    {
        /* Night at the Museum */
		vgmstream->coding_type = coding_NGC_DSP;
		vgmstream->layout_type = layout_interleave_byte;
        vgmstream->interleave_block_size = 2;
    }
    else if (read_32bitLE(0x14,streamFile)==0x10 ||
             read_32bitLE(0x14,streamFile)==0x30)
    {
        /* de Blob, NatM sfx */
        vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->layout_type = layout_none;
        vgmstream->interleave_block_size = read_32bitLE(0x54,streamFile)/channel_count;
    }
    else goto fail;
    vgmstream->num_samples = (read_32bitLE(0x54,streamFile)/8/channel_count*14);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x50,streamFile);
    }
    break;
        /* Night at the Museum */
        case 0x20000882:
        case 0x20000802:
        case 0x20100002:
        case 0x20100802:
        case 0x20100082:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = (read_32bitLE(0x54,streamFile)/8/channel_count*14);
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = read_32bitLE(0x50,streamFile);
            }
            break;
		default:
			goto fail;

	}

    start_offset = read_32bitLE(0x08,streamFile)+0x30;

    vgmstream->meta_type = meta_FSB4;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int c,i;
        for (c=0;c<channel_count;c++) {
            for (i=0;i<16;i++)
            {
                vgmstream->ch[c].adpcm_coef[i] =
                    read_16bitBE(0x80+c*0x2e + i*2,streamFile);
            }
        }
    }
    
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


/* FSB4 with "WAV" Header, found in "Deadly Creatures (WII)"
    16 byte "WAV" header which holds the filesize...*/
VGMSTREAM * init_vgmstream_fsb4_wav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
	int loop_flag;
	int channel_count;
    int fsb_headerlength;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x00574156) /* 0x0\"WAV" */
        goto fail;
    if (read_32bitBE(0x10,streamFile) != 0x46534234) /* "FSB4" */
        goto fail;

	channel_count = (uint16_t)read_16bitLE(0x7E,streamFile);

    if (channel_count > 2) {
        goto fail;
    }
    
    loop_flag = (read_32bitBE(0x70,streamFile) == 0x40000802);
    fsb_headerlength = read_32bitLE(0x18,streamFile);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = fsb_headerlength + 0x40;
	vgmstream->sample_rate = read_32bitLE(0x74,streamFile);
	vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave_byte;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->num_samples = (read_32bitLE(0x64,streamFile)/8/channel_count*14);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x60,streamFile);
	}

    vgmstream->meta_type = meta_FSB4_WAV;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x90+i*2,streamFile);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0xBE + i*2,streamFile);
            }
        }
    }
    
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


