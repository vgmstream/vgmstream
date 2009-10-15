#include "meta.h"
#include "../util.h"

/* HLWAV (from Half Life 2 [XBOX]) */
VGMSTREAM * init_vgmstream_xbox_hlwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("hlwav",filename_extension(filename))) goto fail;
    
    /* check header and size */
    if ((read_32bitBE(0x00,streamFile) != 0x14000000)) goto fail;
    if (((read_32bitLE(0x4,streamFile) + (read_32bitLE(0x8,streamFile))) != get_streamfile_size(streamFile))) goto fail;

    loop_flag = (read_32bitLE(0xC,streamFile)!= 0xFFFFFFFF);
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = read_32bitLE(0x8,streamFile);
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 22050;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = read_32bitLE(0x4,streamFile)/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x4,streamFile)/2/channel_count;
        vgmstream->loop_end_sample = read_32bitLE(0xC,streamFile)/2/channel_count;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_XBOX_HLWAV;

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
