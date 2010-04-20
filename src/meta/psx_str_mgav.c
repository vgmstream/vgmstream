#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* STR (Future Cop L.A.P.D.) */
VGMSTREAM * init_vgmstream_psx_mgav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
		off_t start_offset;
		off_t current_chunk;
    char filename[260];
    int loop_flag = 0;
		int channel_count;
    int dataBuffer = 0;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("str",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52565753) /* "RVWS" */
        goto fail;

    loop_flag = 1;
    channel_count = 2;
    
		/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

		/* fill in the vital statistics */
    start_offset = read_32bitLE(0x4,streamFile);
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = 16000;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_psx_mgav_blocked;
    vgmstream->meta_type = meta_PSX_MGAV;

    /* open the file for reading */
    {
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
		}
	}
	
        // calculate samples
        current_chunk = start_offset;
        vgmstream->num_samples = 0;
        while ((current_chunk + start_offset) < (get_streamfile_size(streamFile)))
        {
          dataBuffer = (read_32bitBE(current_chunk,streamFile));
          if (dataBuffer == 0x4D474156) /* "MGAV" */
          {
            psx_mgav_block_update(start_offset,vgmstream);
            vgmstream->num_samples += vgmstream->current_block_size/16*28;
            current_chunk += vgmstream->current_block_size + 0x1C;
          }
          current_chunk += 0x10;
        }


    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }


    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
