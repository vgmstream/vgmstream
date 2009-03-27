#include "meta.h"
#include "../util.h"

/* WSD - Phantom Brave (WII) */
VGMSTREAM * init_vgmstream_wii_wsd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag;
    int channel_count;
    int coef1_start;
    int coef2_start;
    int second_channel_start;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wsd",filename_extension(filename))) goto fail;

    /* check header, first file should alwas start at 0x20 */
    if (read_32bitBE(0x00,streamFile) != 0x00000020) /* 0x20 */
        goto fail;

    loop_flag = (read_32bitBE(0x2C,streamFile) != 0x0);
    channel_count = read_32bitBE(0x38,streamFile);
        
        coef1_start = 0x3C;
        coef2_start = (read_32bitBE(0x4,streamFile))+0x1C;
        second_channel_start = (read_32bitBE(0x4,streamFile))+0x60;
        
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x80;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x28,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x24,streamFile)/channel_count/8*14;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x30,streamFile)/channel_count/8*14;
        vgmstream->loop_end_sample = read_32bitBE(0x34,streamFile)/channel_count/8*14;
    }

    /* no interleave, we have 2 dsp files in 1 file here */
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_WII_WSD;

    /* Retrieves the coef tables */
    {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(coef1_start+i*2,streamFile);
        }
        if (vgmstream->channels) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(coef2_start +i*2,streamFile);
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
                vgmstream->ch[i].offset=start_offset+second_channel_start;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
