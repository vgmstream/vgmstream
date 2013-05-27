#include "meta.h"
#include "../util.h"

/* .PAST (Bakugan Battle Brawlers */
VGMSTREAM * init_vgmstream_ps3_past(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("past",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x0,streamFile) != 0x534E4450) /* SNDP */
        goto fail;

    loop_flag = (read_32bitBE(0x1C,streamFile)!=0);
    channel_count = (uint16_t)read_16bitBE(0xC,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	start_offset = 0x30;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = (read_32bitBE(0x14,streamFile))/2/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x18,streamFile)/2/channel_count;
        vgmstream->loop_end_sample = read_32bitBE(0x1C,streamFile)/2/channel_count;
    }

    if (channel_count == 1)
    {
		  vgmstream->layout_type = layout_none;
    }
    else
    {
      vgmstream->layout_type = layout_interleave;
		  vgmstream->interleave_block_size = 0x2;
    }

      vgmstream->meta_type = meta_PS3_PAST;

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
