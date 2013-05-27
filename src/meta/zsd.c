#include "meta.h"
#include "../util.h"

/* ZSD (Dragon Booster) */
VGMSTREAM * init_vgmstream_zsd(STREAMFILE *streamFile) {

	VGMSTREAM * vgmstream = NULL;
	char filename[PATH_LIMIT];
	off_t start_offset;

    int loop_flag;
    int channel_count;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile,filename,sizeof(filename));
	if (strcasecmp("zsd",filename_extension(filename))) goto fail;

	/* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5A534400) goto fail;

    loop_flag = 0;
    channel_count = 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = read_32bitLE(0x20,streamFile);
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM8;
    vgmstream->num_samples = read_32bitLE(0x18,streamFile)/channel_count;
    vgmstream->interleave_block_size=0x0;

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_ZSD;


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
