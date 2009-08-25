#include "meta.h"
#include "../util.h"

/* GCUB - found in 'Sega Soccer Slam' */
VGMSTREAM * init_vgmstream_maxis_xa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("xa",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58414900) /* "XAI\0" */
        goto fail;
        
    loop_flag = 0;
    channel_count = read_16bitLE(0xA,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x18;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->coding_type = coding_EA_ADPCM;
    vgmstream->num_samples = vgmstream->num_samples = (int32_t)((get_streamfile_size(streamFile)-start_offset)* 2 / vgmstream->channels);

    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (read_32bitBE(0x0C,streamFile)-start_offset)/8/channel_count*14;
    }

    vgmstream->layout_type = layout_none;
    // vgmstream->interleave_block_size = 0x8000; // read_32bitBE(0x04,streamFile);
    vgmstream->meta_type = meta_MAXIS_XA;


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
