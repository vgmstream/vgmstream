#include "meta.h"
#include "../util.h"

/* ZWDSP (hcs' custom DSP files from Zack & Wiki) */
VGMSTREAM * init_vgmstream_zwdsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    int second_channel_start;
    int loop_flag;
    int channel_count = 2;
    off_t start_offset;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("zwdsp",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x00000000) /* 0x0 */
        goto fail;

    /* Retrieve the loop flag, some files have 0x0 and some 0x02 as "no loop" */

    switch (read_32bitBE(0x10, streamFile)) {
        case 0:
        case 2:
            loop_flag = 0;
            break;
        default:
            loop_flag = 1;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x90;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = read_32bitBE(0x18,streamFile)*14/8/channel_count;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x10,streamFile)*14/8/channel_count;
        vgmstream->loop_end_sample = read_32bitBE(0x14,streamFile)*14/8/channel_count;
        // frequently end sample is one more than total samples in file
        if (vgmstream->loop_end_sample > vgmstream->num_samples)
        {
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }


    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_ZWDSP;


    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i;
        for (i=0;i<16;i++) {
            vgmstream->ch[0].adpcm_coef[i] = read_16bitBE(0x20+i*2,streamFile);
        }
        if (vgmstream->channels == 2) {
            for (i=0;i<16;i++) {
                vgmstream->ch[1].adpcm_coef[i] = read_16bitBE(0x60+i*2,streamFile);
            }
        }
    }

    second_channel_start = (get_streamfile_size(streamFile)+start_offset)/2;
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
