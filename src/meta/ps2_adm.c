#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* WAD (from The golden Compass) */
VGMSTREAM * init_vgmstream_ps2_adm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag = 0;
		int channel_count;
    int i;
		off_t start_offset;
	
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("adm",filename_extension(filename))) goto fail;

    loop_flag = 0;
    channel_count = 2;
    
		/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

		/* fill in the vital statistics */
    start_offset = 0x0;
		vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;

#if 0
    vgmstream->num_samples = read_32bitLE(0x0,streamFile)/channel_count/16*28;
    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = read_32bitLE(0x0,streamFile)/channel_count/16*28;
    }
#endif

      vgmstream->layout_type = layout_ps2_adm_blocked;
      vgmstream->interleave_block_size = 0x400;


    vgmstream->meta_type = meta_PS2_ADM;
    
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,vgmstream->interleave_block_size);
            
        if (!vgmstream->ch[i].streamfile) goto fail;
            vgmstream->ch[i].channel_start_offset=
            vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
        }
    }


    /* Calc num_samples */
    ps2_adm_block_update(start_offset,vgmstream);
    vgmstream->num_samples=0; //(get_streamfile_size(streamFile)/0x1000*0xFE0)/32*28;

    do {
    
     vgmstream->num_samples += 0xFE0*14/16;
        ps2_adm_block_update(vgmstream->next_block_offset,vgmstream);
    } while (vgmstream->next_block_offset<get_streamfile_size(streamFile));

    ps2_adm_block_update(start_offset,vgmstream);

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
