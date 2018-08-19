#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_dsp_bdsp(STREAMFILE *streamFile) {

    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int channel_count;
    int loop_flag;
    int i;
    off_t start_offset;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("bdsp",filename_extension(filename))) goto fail;

    channel_count = 2;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x8,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;

#if 0
    if(loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x64,streamFile);
        vgmstream->loop_end_sample = read_32bitBE(0x68,streamFile);
    }	
#endif


        vgmstream->layout_type = layout_blocked_bdsp;
        vgmstream->interleave_block_size = 0x8;
        vgmstream->meta_type = meta_DSP_BDSP;
    
    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            
        if (!vgmstream->ch[i].streamfile) goto fail;
            vgmstream->ch[i].channel_start_offset=
            vgmstream->ch[i].offset=i*vgmstream->interleave_block_size;
        }
    }

    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x1C+i*2,streamFile);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x7C+i*2,streamFile);
            }
        }
    }

    /* Calc num_samples */
    start_offset = 0x0;
    block_update_bdsp(start_offset,vgmstream);
    vgmstream->num_samples=0;

    do
    {
      vgmstream->num_samples += vgmstream->current_block_size*14/8;
      block_update_bdsp(vgmstream->next_block_offset,vgmstream);
    }
    while (vgmstream->next_block_offset<get_streamfile_size(streamFile));

    block_update_bdsp(start_offset,vgmstream);


    return vgmstream;


    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
