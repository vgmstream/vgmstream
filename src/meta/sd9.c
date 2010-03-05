#include "meta.h"
#include "../util.h"

/* SD9 (found in beatmaniaIIDX16 - EMPRESS (Arcade) */
VGMSTREAM * init_vgmstream_sd9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sd9",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x53443900) /* SD9 */
		goto fail;
	if (read_32bitBE(0x20,streamFile) != 0x52494646) /* RIFF */
		goto fail;
	if (read_32bitBE(0x28,streamFile) != 0x57415645) /* WAVE */
		goto fail;
	if (read_32bitBE(0x2c,streamFile) != 0x666D7420) /* fmt */
		goto fail;
    if (read_32bitBE(0x72,streamFile) != 0x64617461) /* data */
		goto fail;

    loop_flag = (read_16bitLE(0x0e,streamFile)==0x1);
    channel_count = read_16bitLE(0x36,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x7a;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x38,streamFile);
    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->num_samples = read_32bitLE(0x6e,streamFile);
	if (loop_flag) {
        if (read_16bitLE(0x1C,streamFile)==1)
        {
            vgmstream->loop_start_sample = read_32bitLE(0x14,streamFile)/2/channel_count;
            vgmstream->loop_end_sample = read_32bitLE(0x18,streamFile)/2/channel_count;
        }
        else
        {
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }
    vgmstream->layout_type = layout_none;
	vgmstream->interleave_block_size = read_16bitLE(0x40,streamFile);
    vgmstream->meta_type = meta_SD9;

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
