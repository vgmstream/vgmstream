#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

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


/* FSB4 */
VGMSTREAM * init_vgmstream_fsb4(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int fsb4_format;
    int loop_flag = 0;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename)) &&
        strcasecmp("wii",filename_extension(filename))) goto fail;

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

#if 0
    if (read_32bitBE(0x60,streamFile) != 0x20000882 &&
        read_32bitBE(0x60,streamFile) != 0x20100002 &&
        read_32bitBE(0x60,streamFile) != 0x20100882 &&
        read_32bitBE(0x60,streamFile) != 0x20100802 &&
        read_32bitBE(0x60,streamFile) != 0x20100082 &&
        read_32bitBE(0x60,streamFile) != 0x20000802) {
        channel_count = 2;
    } else {
        channel_count = 1;
    }
#endif

    channel_count = (uint16_t)read_16bitLE(0x6E,streamFile);

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
        case 0x20008800: // Silent Hill: Shattered Memories
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
        case 0x40200802:
            if (loop_flag) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = read_32bitLE(0x50,streamFile);
            }

            if (read_32bitLE(0x14,streamFile)==0x20 ||
                read_32bitLE(0x14,streamFile)==0x00)
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
            else if (read_32bitLE(0x14,streamFile)==0x40) {
                /* M. Night Shamylan The Last Airbender */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_interleave_byte;
                vgmstream->interleave_block_size = 2;

                if (loop_flag) {
                    vgmstream->loop_start_sample = read_32bitLE(0x58,streamFile);
                }
            }
            else goto fail;

            vgmstream->num_samples = (read_32bitLE(0x54,streamFile)/8/channel_count*14);
            break;

        /* Night at the Museum */
        case 0x20000882:
        case 0x20000802:
        case 0x20100002:
        case 0x20100882:
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

        /* Rocket Knight (PC) */
	    case 0x50210000:
		case 0x30210000:
		case 0x30011000:
		case 0x20005000:
		case 0x30011080:
		case 0x30211000:
		case 0x40005020:
		case 0x20204000:
		case 0x40204020:
		case 0x50011000:
		case 0x20205000:
		case 0x30610080:
		case 0x50210080: /* Another Century's Episode R (PS3) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;
            vgmstream->num_samples = (read_32bitLE(0x5C,streamFile));
            if (loop_flag) {
                vgmstream->loop_start_sample = read_32bitLE(0x58,streamFile);
                vgmstream->loop_end_sample = read_32bitLE(0x5C,streamFile);
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


// FSB MPEG TEST
VGMSTREAM * init_vgmstream_fsb_mpeg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int channel_count;
    int loop_flag;
    long sample_rate = 0;
    long num_samples = 0;
    uint8_t mp3ID;
    long rate;
    int channels,encoding;
    // int FSBFlag;

#ifdef VGM_USE_MPEG
    mpeg_codec_data *mpeg_data = NULL;
    coding_t mpeg_coding_type = coding_MPEG1_L3;
#endif

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x46534234) /* "FSB4" */
        goto fail;
 
    /* "Check if the FSB is used as conatiner or as single file" */
    if (read_32bitBE(0x04,streamFile) != 0x01000000)
        goto fail;
    
    /* Check channel count, multi-channel not supported and will be refused */
    if ((read_16bitLE(0x6E,streamFile) != 0x2) &&
       (read_16bitLE(0x6E,streamFile) != 0x1))
        goto fail;

		start_offset = read_32bitLE(0x08,streamFile)+0x30; //read_32bitBE(0x4,streamFile);
    mp3ID = read_8bit(start_offset,streamFile);
    if (mp3ID != 0xFF)
        goto fail;
     
	/* Still WIP */
	if (read_32bitBE(0x80,streamFile)==0x53594E43)
        loop_flag = 1;
	else
		loop_flag = 0;

    num_samples = (read_32bitLE(0x5C,streamFile));

#ifdef VGM_USE_MPEG
        mpeg_data = init_mpeg_codec_data(streamFile, start_offset, -1, -1, &mpeg_coding_type); // -1 to not check sample rate or channels
        if (!mpeg_data) goto fail;

        if (MPG123_OK != mpg123_getformat(mpeg_data->m,&rate,&channels,&encoding)) goto fail;
        channel_count = channels;
        sample_rate = rate;

#else
        // reject if no MPEG support
        goto fail;
#endif

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->channels = channel_count;
  
	/* Still WIP */
	if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x58,streamFile);
       vgmstream->loop_end_sample = read_32bitLE(0x5c,streamFile);
    }
    vgmstream->meta_type = meta_FSB_MPEG;

#ifdef VGM_USE_MPEG
        /* NOTE: num_samples seems to be quite wrong for MPEG */
        vgmstream->codec_data = mpeg_data;
		vgmstream->layout_type = layout_mpeg;
		vgmstream->coding_type = mpeg_coding_type;
#else
        // reject if no MPEG support
        goto fail;
#endif


#if 0
	if (loop_flag) {
			vgmstream->loop_start_sample = read_32bitBE(0x18,streamFile)/960*1152;
			vgmstream->loop_end_sample = read_32bitBE(0x1C,streamFile)/960*1152;
  }
#endif

    /* open the file for reading */
    {
    int i;
      STREAMFILE * file;
        if(vgmstream->layout_type == layout_interleave)
        {
          file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
			    if (!file) goto fail;
			        for (i=0;i<channel_count;i++)
              {
				        vgmstream->ch[i].streamfile = file;
        				vgmstream->ch[i].channel_start_offset=
				      	vgmstream->ch[i].offset=start_offset+
					      vgmstream->interleave_block_size*i;
              }
        }

#ifdef VGM_USE_MPEG
		else if(vgmstream->layout_type == layout_mpeg) {
			for (i=0;i<channel_count;i++) {
				vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,MPEG_BUFFER_SIZE);
				vgmstream->ch[i].channel_start_offset= vgmstream->ch[i].offset=start_offset;
      }

    }
#endif
        else { goto fail; }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
#ifdef VGM_USE_MPEG
    if (mpeg_data) {
        mpg123_delete(mpeg_data->m);
        free(mpeg_data);

        if (vgmstream) {
            vgmstream->codec_data = NULL;
        }
    }
#endif
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
