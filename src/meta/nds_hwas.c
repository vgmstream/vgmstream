#include "meta.h"
#include "../util.h"

/* HWAS (found in Spider-Man 3, Tony Hawk's Downhill Jam, possibly more...) */
VGMSTREAM * init_vgmstream_nds_hwas(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("hwas",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x73617768) /* "sawh" */
        goto fail;

    loop_flag = 0;
    channel_count = read_32bitLE(0x0C,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x200;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->coding_type = coding_INT_IMA;
    vgmstream->num_samples = read_32bitLE(0x14,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x10,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(0x18,streamFile);
    }

    if (channel_count == 1) {
        vgmstream->layout_type = layout_none;
    } else {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x10; // Not sure if there are stereo files
    }

    vgmstream->meta_type = meta_NDS_HWAS;

    /* open the file for reading by each channel */
      
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
