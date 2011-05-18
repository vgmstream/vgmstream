#include "meta.h"
#include "../util.h"

/* TUN (from LEGO Racers (PC)) */
VGMSTREAM * init_vgmstream_tun(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int channel_count;
	int loop_flag;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("tun",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x414C5020) /* "ALP " */
        goto fail;

	channel_count = 2;
	loop_flag = 0;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x10;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 22050;
    vgmstream->coding_type = coding_DVI_IMA;
    vgmstream->num_samples = (get_streamfile_size(streamFile)) - 0x10;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 1;
    vgmstream->meta_type = meta_TUN;

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
