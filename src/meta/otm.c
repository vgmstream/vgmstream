#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"

/* Otomedius OTM (Arcade) */
VGMSTREAM * init_vgmstream_otm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("otm",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x20,streamFile) != 0x10B10200)
		goto fail;
	if (read_32bitBE(0x24,streamFile) != 0x04001000)
        goto fail;

	if (read_32bitBE(0x14,streamFile) != 0x00000000)
    loop_flag = 1;
	else
		loop_flag = 0;
    channel_count = read_16bitLE(0x1A,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = 0x2C;
	vgmstream->num_samples = (get_streamfile_size(streamFile)- start_offset)/channel_count/2;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x1C,streamFile);
	    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitLE(0x10,streamFile))/channel_count/2;
       vgmstream->loop_end_sample = (read_32bitLE(0xC,streamFile) - start_offset)/channel_count/2;
    }
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave; 
    vgmstream->interleave_block_size = 2;
    vgmstream->meta_type = meta_OTM;

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

