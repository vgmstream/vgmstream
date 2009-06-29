#include "meta.h"
#include "../util.h"

/* ADS (from Gauntlet Dark Legends (GC)) */
VGMSTREAM * init_vgmstream_ads(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag;
    int channel_count;
    int identifer_byte;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ads",filename_extension(filename))) goto fail;

    /* check dhSS Header */
    if (read_32bitBE(0x00,streamFile) != 0x64685353)
        goto fail;

    /* check dbSS Header */
    if (read_32bitBE(0x20,streamFile) != 0x64625353)
        goto fail;
    
    loop_flag = 1;
    channel_count = read_32bitBE(0x10,streamFile);

    if (channel_count > 0x2)
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    identifer_byte = read_32bitBE(0x08,streamFile);
    switch (identifer_byte) {
        case 0x00000020:
            start_offset = 0xE8;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->num_samples = read_32bitBE(0x28,streamFile);
        if (loop_flag) {
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
        
        if (channel_count == 1){
            vgmstream->layout_type = layout_none;
        } else if (channel_count == 2){
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = read_32bitBE(0x14,streamFile);
        }
    break;
        case 0x00000021:
            start_offset = 0x28;
            vgmstream->channels = channel_count;
            vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
            vgmstream->coding_type = coding_INT_XBOX;
            vgmstream->num_samples = (read_32bitBE(0x24,streamFile) / 36 *64 / vgmstream->channels)-64; // to avoid the "pop" at the loop point
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x24;
        if (loop_flag) {
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
        break;
    default:
goto fail;
}

    vgmstream->meta_type = meta_ADS;

        {
        int i;
        for (i=0;i<16;i++)
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x44+i*2,streamFile);
        if (channel_count == 2) {
        for (i=0;i<16;i++)
            vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0xA4+i*2,streamFile);
    }
        }


    /* open the file for reading */
    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i,c;
        for (c=0;c<channel_count;c++) {
            for (i=0;i<16;i++) {
                vgmstream->ch[c].adpcm_coef[i] =
                    read_16bitBE(0x44+c*0x60 +i*2,streamFile);
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

            
            vgmstream->ch[i].channel_start_offset=
                start_offset+vgmstream->interleave_block_size*i;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset;

        }
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
