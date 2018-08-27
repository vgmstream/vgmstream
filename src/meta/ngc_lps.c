#include "meta.h"
#include "../util.h"

/* LPS (found in Rave Master (Groove Adventure Rave)(GC) */
VGMSTREAM * init_vgmstream_ngc_lps(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
	int loop_flag;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("lps",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x8,streamFile) != 0x10000000)
		goto fail;

    loop_flag = read_32bitBE(0x30,streamFile);
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  start_offset = 0x60;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x28,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (read_32bitBE(0x34,streamFile))/16*14;
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0x30,streamFile))/16*14;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NGC_LPS;
    vgmstream->allow_dual_stereo = 1;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x3C+i*2,streamFile);
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

	/* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
