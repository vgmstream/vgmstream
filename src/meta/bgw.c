#include "meta.h"
#include "../util.h"

/* BGW (FF XI) */
VGMSTREAM * init_vgmstream_bgw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bgw",filename_extension(filename))) goto fail;

    /* "BGMStream" */
    if (read_32bitBE(0,streamFile) != 0x42474d53 ||
        read_32bitBE(4,streamFile) != 0x74726561 ||
        read_32bitBE(8,streamFile) != 0x6d000000 |
        read_32bitBE(12,streamFile) != 0) goto fail;

    /* check file size with header value */
    if (read_32bitLE(0x10,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    channel_count = 2;
    loop_flag = read_32bitLE(0x1c,streamFile) != 0;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x30;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_FFXI;
    vgmstream->num_samples = read_32bitLE(0x18,streamFile)*16;
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitLE(0x1c,streamFile)-1)*16;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 9;
    vgmstream->meta_type = meta_FFXI_BGW;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset+i*9;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
