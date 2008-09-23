#include "meta.h"
#include "../util.h"

/* sadl (only the Professor Layton interleaved IMA version) */
VGMSTREAM * init_vgmstream_sadl(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("sad",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x7361646c) /* "sadl" */
        goto fail;

    /* check file size */
    if (read_32bitLE(0x40,streamFile) != get_streamfile_size(streamFile) )
        goto fail;

    /* check for the simple IMA type that we can handle */
    if (read_8bit(0xc,streamFile) != 0x11)
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x100;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 16364;
    vgmstream->coding_type = coding_INT_IMA;
    vgmstream->num_samples = read_32bitLE(0x50,streamFile);;
    vgmstream->interleave_block_size=0x10;

    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_SADL;

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
