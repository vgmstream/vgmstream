#include "meta.h"
#include "../util.h"

/* RSD */
/* RSD2VAG */
VGMSTREAM * init_vgmstream_rsd2vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534432) /* RSD2 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x56414720)	/* VAG\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);
    vgmstream->meta_type = meta_RSD2VAG;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* RSD2PCMB - Big Endian */
VGMSTREAM * init_vgmstream_rsd2pcmb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534432) /* RSD2 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x50434D42)	/* PCMB */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = read_32bitLE(0x18,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    }

	
	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count == 2) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x2;
	}


    vgmstream->meta_type = meta_RSD2PCMB;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/* RSD2XADP */
VGMSTREAM * init_vgmstream_rsd2xadp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534432) /* RSD2 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x58414450)	/* XADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = read_32bitLE(0x18,streamFile); /* not sure about this */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_XBOX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)*64/36/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSD2XADP;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

   
		if (vgmstream->coding_type == coding_XBOX) {
				vgmstream->layout_type=layout_none;
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    start_offset+vgmstream->interleave_block_size*i;
            }
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;

        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* RSD3VAG */
VGMSTREAM * init_vgmstream_rsd3vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534433) /* RSD3 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x56414720)	/* VAG\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);
    vgmstream->meta_type = meta_RSD3VAG;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* RSD3GADP */
VGMSTREAM * init_vgmstream_rsd3gadp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534433) /* RSD3 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x47414450)	/* WADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = read_32bitLE(0x18,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSD3GADP;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x1D+i*2,streamFile);
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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* RSD3PCM  - Little Endian */
VGMSTREAM * init_vgmstream_rsd3pcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534433) /* RSD3 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x50434D20)	/* PCM\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = read_32bitLE(0x18,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    }

	
	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count == 2) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x2;
	}

	vgmstream->meta_type = meta_RSD3PCM;


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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/* RSD3PCMB - Big Endian */
VGMSTREAM * init_vgmstream_rsd3pcmb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534433) /* RSD3 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x50434D42)	/* PCMB */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = read_32bitLE(0x18,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)/2/channel_count;
    }

	
	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count == 2) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x2;
	}


    vgmstream->meta_type = meta_RSD3PCMB;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/* RSD4VAG */
VGMSTREAM * init_vgmstream_rsd4vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534434) /* RSD4 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x56414720)	/* VAG\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0xC,streamFile);
    vgmstream->meta_type = meta_RSD4VAG;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* RSD4PCM  - Little Endian */
VGMSTREAM * init_vgmstream_rsd4pcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534434) /* RSD4 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x50434D20)	/* PCM\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)/2/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_RSD4PCM;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/* RSD4PCMB - Big Endian */
VGMSTREAM * init_vgmstream_rsd4pcmb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534434) /* RSD4 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x50434D42)	/* PCMB */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x80;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)/2/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_RSD4PCMB;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* RSD4RADP */
VGMSTREAM * init_vgmstream_rsd4radp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534434) /* RSD4 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x52414450)	/* RADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_RAD_IMA;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/0x14/channel_count*32;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)*28/16/channel_count;
    }
    vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = 0x14*channel_count;
    vgmstream->meta_type = meta_RSD4RADP;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
   
            vgmstream->ch[i].offset=vgmstream->ch[i].channel_start_offset=start_offset;
        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* RSD6RADP */
VGMSTREAM * init_vgmstream_rsd6radp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x52414450)	/* RADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_RAD_IMA;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/0x14/channel_count*32;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)*28/16/channel_count;
    }
    vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = 0x14*channel_count;
    vgmstream->meta_type = meta_RSD6RADP;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
   
            vgmstream->ch[i].offset=vgmstream->ch[i].channel_start_offset=start_offset;
        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* RSD6VAG */
VGMSTREAM * init_vgmstream_rsd6vag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x56414720)	/* VAG\0x20 */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0xC,streamFile);
    vgmstream->meta_type = meta_RSD6VAG;

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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


/* RSD6WADP */
VGMSTREAM * init_vgmstream_rsd6wadp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x57414450)	/* WADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-0x800)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave_byte; //layout_interleave;
    vgmstream->interleave_block_size = 2; //read_32bitLE(0xC,streamFile);
    vgmstream->meta_type = meta_RSD6WADP;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x1A4+i*2,streamFile);
        }
        if (vgmstream->channels) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x1CC+i*2,streamFile);
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

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* RSD6OGG */
VGMSTREAM * init_vgmstream_rsd6oogv(STREAMFILE *streamFile) {
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x4F4F4756)	/* OOGV */
        goto fail;

  	#ifdef VGM_USE_VORBIS
    {
        vgm_vorbis_info_t inf;
        VGMSTREAM * result = NULL;

        memset(&inf, 0, sizeof(inf));
        inf.layout_type = layout_ogg_vorbis;
        inf.meta_type = meta_RSD6OOGV;

        start_offset = 0x800;
		result = init_vgmstream_ogg_vorbis_callbacks(streamFile, filename, NULL, start_offset, &inf);

        if (result != NULL) {
            return result;
        }
	   }
#endif

fail:
    /* clean up anything we may have opened */
    return NULL;
}

/* RSD6XADP */
VGMSTREAM * init_vgmstream_rsd6xadp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x58414450)	/* XADP */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_XBOX;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)*64/36/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)*28/16/channel_count;
    }
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSD6XADP;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

   
		if (vgmstream->coding_type == coding_XBOX) {
				vgmstream->layout_type=layout_none;
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    start_offset+vgmstream->interleave_block_size*i;
            }
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;

        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

#if 0 
/* RSD6XMA */
VGMSTREAM * init_vgmstream_rsd6xma(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("rsd",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x52534436) /* RSD6 */
		goto fail;
	if (read_32bitBE(0x4,streamFile) != 0x584D4120)	/* XMA */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x8,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x800;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_XMA;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)*64/36/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_flag;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset)*28/16/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_RSD6XMA;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

   
		if (vgmstream->coding_type == coding_XMA) {
				vgmstream->layout_type=layout_none;
                vgmstream->ch[i].channel_start_offset=start_offset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    start_offset+vgmstream->interleave_block_size*i;
            }
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;

        }
    }
    
	return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
} 

#endif
