#include "meta.h"
#include "../util.h"

/* PONA (from Policenauts [3DO and PSX]) */
VGMSTREAM * init_vgmstream_pona(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("pona",filename_extension(filename))) goto fail;

    /* check header */
    if (((read_32bitBE(0x00,streamFile) != 0x13020000) ||
        (((uint16_t)read_16bitBE(0x04,streamFile)) != 0x0800))) /* 0x0800 */
    goto fail;

    loop_flag = (read_32bitBE(0x0A,streamFile) != 0xFFFFFFFF);
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = (uint16_t)(read_16bitBE(0x04,streamFile));
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 22050;
    vgmstream->coding_type = coding_SDX2;
    vgmstream->num_samples = (get_streamfile_size(streamFile))-start_offset;
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0x0A,streamFile));
        vgmstream->loop_end_sample = (read_32bitBE(0x06,streamFile));
    }

    vgmstream->layout_type = layout_none;
    // vgmstream->interleave_block_size = 0x1;
    vgmstream->meta_type = meta_PONA;

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
