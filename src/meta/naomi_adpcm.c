#include "meta.h"
#include "../util.h"

/* ADPCM (from NAOMI/NAOMI2 Arcade games) */
VGMSTREAM * init_vgmstream_naomi_adpcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("adpcm",filename_extension(filename))) goto fail;

#if 0
    /* check header */
    if ((read_32bitBE(0x00,streamFile) != 0x41445043) ||   /* "ADPC" */
        (read_32bitBE(0x04,streamFile) != 0x41445043))  /* "M_v0" */
    goto fail;
#endif

    loop_flag = 0;
    channel_count = 2;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x40;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_AICA;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset);
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = (get_streamfile_size(streamFile)-start_offset);
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x10,streamFile) * 0x80;
    vgmstream->meta_type = meta_NAOMI_ADPCM;

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
                vgmstream->ch[i].adpcm_step_index = 0x7f;   /* AICA */
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
