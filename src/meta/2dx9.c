#include "meta.h"
#include "../util.h"

/* 2DX9 (found in beatmaniaIIDX16 - EMPRESS (Arcade) */
VGMSTREAM * init_vgmstream_2dx9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("2dx9",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x32445839) /* 2DX9 */
		goto fail;
	if (read_32bitBE(0x18,streamFile) != 0x52494646) /* RIFF */
		goto fail;
	if (read_32bitBE(0x20,streamFile) != 0x57415645) /* WAVE */
		goto fail;
	if (read_32bitBE(0x24,streamFile) != 0x666D7420) /* fmt */
		goto fail;
    if (read_32bitBE(0x6a,streamFile) != 0x64617461) /* data */
		goto fail;

    loop_flag = 0;
    channel_count = read_16bitLE(0x2e,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x72;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x30,streamFile);
    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->num_samples = read_32bitLE(0x66,streamFile);
    vgmstream->layout_type = layout_none;
	vgmstream->interleave_block_size = read_16bitLE(0x38,streamFile);
    vgmstream->meta_type = meta_2DX9;

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
