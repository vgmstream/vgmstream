#include "meta.h"
#include "../util.h"

/* .dsp found in Yu-Gi-Oh! The Falsebound Kingdom (GC) */
VGMSTREAM * init_vgmstream_dsp_ygo(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("dsp",filename_extension(filename))) goto fail;

    /* check header */
	if (read_32bitBE(0x10,streamFile) != 0x0000)
        goto fail;

    loop_flag = 0;
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0xE0;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x28,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x20,streamFile);
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DSP_YGO;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x3c+i*2,streamFile);
        }
    }

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
