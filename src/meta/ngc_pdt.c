#include "meta.h"
#include "../util.h"

/* PDT - Custom Generated File (Mario Party) */
VGMSTREAM * init_vgmstream_ngc_pdt(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag;
    int channel_count;
    off_t start_offset;
    int second_channel_start = -1;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("pdt",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x50445420) /* "PDT " */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x44535020) /* "DSP " */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x48454144) /* "HEAD " */
        goto fail;
    if (read_16bitBE(0x0C,streamFile) != 0x4552) /* "ER " */
        goto fail;

    loop_flag = (read_32bitBE(0x1C,streamFile)!=2);
    channel_count = (uint16_t)(read_16bitLE(0x0E,streamFile));

	  /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	  /* fill in the vital statistics */
    start_offset = 0x800;
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x14,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;

    if (channel_count == 1)
    {
      vgmstream->num_samples = read_32bitBE(0x18,streamFile)*14/8/channel_count/2;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,streamFile)*14/8/channel_count/2;
            vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile)*14/8/channel_count/2;
        }
    }
    else if (channel_count == 2)
    {
      vgmstream->num_samples = read_32bitBE(0x18,streamFile)*14/8/channel_count;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,streamFile)*14/8/channel_count;
            vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile)*14/8/channel_count;
        }
        second_channel_start = (get_streamfile_size(streamFile)+start_offset)/2;
    }
    else
    {
      goto fail;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_NGC_PDT;

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x50+i*2,streamFile);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x70+i*2,streamFile);
            }
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

                vgmstream->ch[0].channel_start_offset=start_offset;
            if (channel_count == 2) {
                if (second_channel_start == -1) goto fail;
                vgmstream->ch[1].channel_start_offset=second_channel_start;
            }
        vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
