#include "meta.h"
#include "../util.h"

/* DMSG
		found in: Nightcaster II - Equinox
		2010-01-05 (manakoAT): Seems it's a corrupted "SGT" file, but I'm not sure...
 */
VGMSTREAM * init_vgmstream_dmsg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
	int loop_flag = 0;
	int frequency;
	int channel_count;
    int dataBuffer = 0;
    int Founddata = 0;
    size_t file_size;
    off_t current_chunk;
	off_t start_offset;
    
	/* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("dmsg",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
		goto fail;
	if (read_32bitBE(0x08,streamFile) != 0x444D5347) /* "DMSG" */
		goto fail;
	if (read_32bitBE(0x0C,streamFile) != 0x73656768) /* "segh" */
		goto fail;
	if (read_32bitBE(0x10,streamFile) != 0x38000000) /* "0x38" */
		goto fail;

	/* scan file until we find a "data" string */
    file_size = get_streamfile_size(streamFile);
    {
        current_chunk = 0;
        /* Start at 0 and loop until we reached the
        file size, or until we found a "data string */
        while (!Founddata && current_chunk < file_size) {
        dataBuffer = (read_32bitBE(current_chunk,streamFile));
            if (dataBuffer == 0x64617461) { /* "data" */
                /* if "data" string found, retrieve the needed infos */
                Founddata = 1;
                /* We will cancel the search here if we have a match */
            break;
            }
            /* else we will increase the search offset by 1 */
            current_chunk = current_chunk + 1;
        }
    }

	if (Founddata == 0) {
		goto fail;
	} else if (Founddata == 1) {
		channel_count = (uint16_t)read_16bitLE(current_chunk-0x10,streamFile);
		frequency = read_32bitLE(current_chunk-0xE,streamFile);
	}

	loop_flag = 1;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	if (Founddata == 0) {
		goto fail;
	} else if (Founddata == 1) {
		start_offset = current_chunk+0x8;
		vgmstream->channels = channel_count;
		vgmstream->sample_rate = frequency;
		vgmstream->coding_type = coding_PCM16LE;
		vgmstream->num_samples = (read_32bitLE(current_chunk+0x4,streamFile)/2/channel_count);
		if (loop_flag) {
			vgmstream->loop_start_sample = 0;
			vgmstream->loop_end_sample = (read_32bitLE(current_chunk+0x4,streamFile)/2/channel_count);
		}
	}
	
	if (channel_count == 1) {
		vgmstream->layout_type = layout_none;
	} else if (channel_count > 1) {
		vgmstream->layout_type = layout_interleave;
		vgmstream->interleave_block_size = 0x2;
	}

	    vgmstream->meta_type = meta_DMSG;

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
