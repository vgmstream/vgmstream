#include "meta.h"
#include "../util.h"


/* MUSX (Version 004) --------------------------------------->*/
VGMSTREAM * init_vgmstream_musx_v004(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
    int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("musx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
		  goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x04000000) /* "0x04000000" */
		  goto fail;

    loop_flag = (read_32bitLE(0x840,streamFile) != 0xFFFFFFFF);
    channel_count = 2;

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  switch (read_32bitBE(0x10,streamFile))
    {
	    case 0x5053325F: /* PS2_ */
			  start_offset = read_32bitLE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 32000;
			  vgmstream->coding_type = coding_PSX; // PS2 ADPCM
			  vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))/16/channel_count*28;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x80;
		      if (loop_flag)
          {
			      vgmstream->loop_start_sample = (read_32bitLE(0x890,streamFile))/16/channel_count*28;
			      vgmstream->loop_end_sample = (read_32bitLE(0x89C,streamFile))/16/channel_count*28;
          }
      break;
		  case 0x47435F5F: /* GC__ */
			  start_offset = read_32bitBE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 32000;
			  vgmstream->coding_type = coding_DAT4_IMA; // Eurocom DAT4 4-bit IMA ADPCM
			  vgmstream->num_samples = (read_32bitBE(0x2C,streamFile))/16/channel_count*28;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x20;
		      if (loop_flag)
          {
			      vgmstream->loop_start_sample = (read_32bitBE(0x890,streamFile))/16/channel_count*28;
			      vgmstream->loop_end_sample = (read_32bitBE(0x89C,streamFile))/16/channel_count*28;
          }
      break;
	    case 0x58425F5F: /* XB__ */
			  start_offset = read_32bitLE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 44100;
			  vgmstream->coding_type = coding_DAT4_IMA; // Eurocom DAT4 4-bit IMA ADPCM
			  vgmstream->num_samples = (read_32bitLE(0x2C,streamFile))/16/channel_count*28;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x20;
		    if (loop_flag)
        {
			    vgmstream->loop_start_sample = (read_32bitLE(0x890,streamFile))/16/channel_count*28;
			    vgmstream->loop_end_sample = (read_32bitLE(0x89C,streamFile))/16/channel_count*28;
        }
	    break;
        default:
          goto fail;
    }

    vgmstream->meta_type = meta_MUSX_V004;
    
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
/* <--------------------------------------- MUSX (Version 004) */


/* MUSX (Version 005) --------------------------------------->*/
VGMSTREAM * init_vgmstream_musx_v005(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
    int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("musx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
		  goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x05000000) /* "0x04000000" */
		  goto fail;

    loop_flag = (read_32bitLE(0x840,streamFile) != 0xFFFFFFFF);
    channel_count = 2;

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  switch (read_32bitBE(0x10,streamFile))
    {
		  case 0x47435F5F: /* GC__ */
			  start_offset = read_32bitBE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 32000;
			  vgmstream->coding_type = coding_DAT4_IMA; // Eurocom DAT4 4-bit IMA ADPCM
			  vgmstream->num_samples = (read_32bitBE(0x2C,streamFile))/16/channel_count*28;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x20;
		      if (loop_flag)
          {
			      vgmstream->loop_start_sample = (read_32bitBE(0x890,streamFile))/16/channel_count*28;
			      vgmstream->loop_end_sample = (read_32bitBE(0x89C,streamFile))/16/channel_count*28;
          }
      break;
        default:
          goto fail;
    }

    vgmstream->meta_type = meta_MUSX_V005;
    
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
/* <--------------------------------------- MUSX (Version 005) */



/* MUSX (Version 006) ---------------------------------------> */
VGMSTREAM * init_vgmstream_musx_v006(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
    int channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("musx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
      goto fail;
    
    if (read_32bitBE(0x08,streamFile) != 0x06000000) /* "0x06000000" */
      goto fail;

    loop_flag = (read_32bitLE(0x840,streamFile)!=0xFFFFFFFF);
    channel_count = 2;

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
      if (!vgmstream) goto fail;

	  /* fill in the vital statistics */	
    switch (read_32bitBE(0x10,streamFile))
    {
      case 0x5053325F: /* PS2_ */
			  start_offset = read_32bitLE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 32000;
			  vgmstream->coding_type = coding_PSX;
			  vgmstream->num_samples = (read_32bitLE(0x0C,streamFile))*28/16/channel_count;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x80;
			  vgmstream->meta_type = meta_MUSX_V006;
        if (loop_flag)
        {
          vgmstream->loop_start_sample = (read_32bitLE(0x890,streamFile))*28/16/channel_count;
          vgmstream->loop_end_sample = (read_32bitLE(0x89C,streamFile))*28/16/channel_count;
        }
	    break;
		  case 0x47435F5F: /* GC__ */
			  start_offset = read_32bitBE(0x28,streamFile);
			  vgmstream->channels = channel_count;
			  vgmstream->sample_rate = 32000;
			  vgmstream->coding_type = coding_DAT4_IMA;
			  vgmstream->num_samples = (read_32bitBE(0x2C,streamFile))*28/16/channel_count;
			  vgmstream->layout_type = layout_interleave;
			  vgmstream->interleave_block_size = 0x20;
			  vgmstream->meta_type = meta_MUSX_V006;
		    if (loop_flag)
        {
          vgmstream->loop_start_sample = (read_32bitBE(0x890,streamFile))*28/16/channel_count;
          vgmstream->loop_end_sample = (read_32bitBE(0x89C,streamFile))*28/16/channel_count;
        }
	    break;
		   default:
			    goto fail;
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
/* <--------------------------------------- MUSX (Version 006) */


/* MUSX (Version 010) --------------------------------------->*/
/* WII_ in Dead Space: Extraction */
VGMSTREAM * init_vgmstream_musx_v010(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
	int musx_type; /* determining the decoder by strings like "PS2_", "GC__" and so on */
	//int musx_version; /* 0x08 provides a "version" byte */
	int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("musx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
		  goto fail;
    if (read_32bitBE(0x800,streamFile) == 0x53424E4B) /* "SBNK", */ // SoundBank, refuse
		  goto fail;
	  if (read_32bitBE(0x08,streamFile) != 0x0A000000) /* "0x0A000000" */
		  goto fail;

	loop_flag = ((read_32bitLE(0x34,streamFile)!=0x00000000) &&
							(read_32bitLE(0x34,streamFile)!=0xABABABAB));
    channel_count = 2;
    
	musx_type=(read_32bitBE(0x10,streamFile));

    if (musx_type == 0x5749495F &&  /* WII_ */
        (read_16bitBE(0x40,streamFile) == 0x4441) && /* DA */
        (read_8bit(0x42,streamFile) == 0x54)) /* T */
    {
        channel_count = read_32bitLE(0x48,streamFile);
        loop_flag = (read_32bitLE(0x64,streamFile) != -1);
    }
	if (musx_type == 0x5053335F &&  /* PS3_ */
        (read_16bitBE(0x40,streamFile) == 0x4441) && /* DA */
        (read_8bit(0x42,streamFile) == 0x54)) /* T */
    {
        channel_count = read_32bitLE(0x48,streamFile);
        loop_flag = (read_32bitLE(0x64,streamFile) != -1);
    }
    if (0x58455F5F == musx_type) /* XE__ */
    {
        loop_flag = 0;
    }

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
	
	/* fill in the vital statistics */	
    switch (musx_type) {
        case 0x5053325F: /* PS2_ */
            start_offset = 0x800;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = 32000;
            vgmstream->coding_type = coding_PSX;
            vgmstream->num_samples = read_32bitLE(0x40,streamFile);
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x80;
            vgmstream->meta_type = meta_MUSX_V010;
            if (loop_flag)
            {
                vgmstream->loop_start_sample = read_32bitLE(0x44,streamFile);
                vgmstream->loop_end_sample = read_32bitLE(0x40,streamFile);
            }
            break;
        case 0x5053505F: /* PSP_ */
            start_offset = 0x800;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = 32768;
            vgmstream->coding_type = coding_PSX;
            vgmstream->num_samples = (read_32bitLE(0xC,streamFile))*28/32;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x80;
            vgmstream->meta_type = meta_MUSX_V010;
            break;
        case 0x5053335F: /* PS3_ */
            start_offset = 0x800;
            vgmstream->channels = channel_count;
            vgmstream->coding_type = coding_DAT4_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x20;
            vgmstream->meta_type = meta_MUSX_V010;

			if (read_32bitBE(0x40,streamFile)==0x44415438){
            vgmstream->num_samples = read_32bitLE(0x60,streamFile);
			vgmstream->sample_rate = read_32bitLE(0x4C,streamFile);
			if (loop_flag)
            {
                vgmstream->loop_start_sample = read_32bitLE(0x64,streamFile);
                vgmstream->loop_end_sample = read_32bitLE(0x60,streamFile);
            }
			}
			else {
				vgmstream->sample_rate = 44100;
				vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)/2/(0x20)*((0x20-4)*2);
		    if (loop_flag)
            {
                vgmstream->loop_start_sample = read_32bitLE(0x44,streamFile);
                vgmstream->loop_end_sample = read_32bitLE(0x40,streamFile);
            }
			}
            break;
        case 0x5749495F: /* WII_ */
            start_offset = 0x800;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = read_32bitLE(0x4C,streamFile);
            switch (read_32bitBE(0x40,streamFile))
            {
                case 0x44415434:    /* DAT4 */
				case 0x44415438:    /* DAT8 */
                    vgmstream->coding_type = coding_DAT4_IMA;
                    break;
                default:
                    goto fail;
            }
            vgmstream->num_samples = read_32bitLE(0x60,streamFile);
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x20;
            vgmstream->meta_type = meta_MUSX_V010;
            if (loop_flag)
            {
                vgmstream->loop_start_sample = read_32bitLE(0x64,streamFile);
                vgmstream->loop_end_sample = read_32bitLE(0x60,streamFile);
            }

            break;
        case 0x58455F5F: /* XE__ */
            start_offset = 0x800;
            vgmstream->sample_rate = 32000;
            vgmstream->coding_type = coding_DAT4_IMA;
            vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)/2/(0x20)*((0x20-4)*2);
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x20;
            vgmstream->meta_type = meta_MUSX_V010;
            
            break;
        default:
            goto fail;
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
/* <--------------------------------------- MUSX (Version 010) */


/* MUSX (Version 201) --------------------------------------->*/
VGMSTREAM * init_vgmstream_musx_v201(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
	//int musx_version; /* 0x08 provides a "version" byte */
	int loop_flag;
	int channel_count;
	int loop_detect;
	int loop_offsets;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("musx",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x4D555358) /* "MUSX" */
		goto fail;
	if ((read_32bitBE(0x08,streamFile) != 0xC9000000) &&
    (read_32bitLE(0x08,streamFile) != 0xC9000000)) /* "0xC9000000" */
		goto fail;

    channel_count = 2;

	loop_detect = read_32bitBE(0x800,streamFile);
	switch (loop_detect) {
		case 0x02000000:
		loop_offsets = 0x8E0;
	break;
		case 0x03000000:
		loop_offsets = 0x880;
	break;
		case 0x04000000:
		loop_offsets = 0x8B4;
	break;
		case 0x05000000:
		loop_offsets = 0x8E8;
	break;
		case 0x06000000:
		loop_offsets = 0x91C;
	break;
		default:
			goto fail;
	}

	loop_flag = (read_32bitLE(loop_offsets+0x04,streamFile) !=0x00000000);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */	
		start_offset = read_32bitLE(0x18,streamFile);
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = 32000;
		vgmstream->coding_type = coding_PSX;
		vgmstream->num_samples = read_32bitLE(loop_offsets,streamFile)*28/16/channel_count;
		if (loop_flag) {
			vgmstream->loop_start_sample = read_32bitLE(loop_offsets+0x10,streamFile)*28/16/channel_count;
			vgmstream->loop_end_sample = read_32bitLE(loop_offsets,streamFile)*28/16/channel_count;
		}
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x80;
		vgmstream->meta_type = meta_MUSX_V201;	
	
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
/* <--------------------------------------- MUSX (Version 201) */
