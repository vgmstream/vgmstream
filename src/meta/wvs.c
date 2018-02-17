#include "meta.h"
#include "../coding/coding.h"

/* WVS - found in Metal Arms - Glitch in the System (Xbox) */
VGMSTREAM * init_vgmstream_xbox_wvs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;


    /* check extension */
    if (!check_extensions(streamFile,"wvs"))
        goto fail;

    if (read_16bitLE(0x0C,streamFile) != 0x69 &&    /* codec */
        read_16bitLE(0x08,streamFile) != 0x4400)
        goto fail;

    start_offset = 0x20;
    data_size = read_32bitLE(0x00,streamFile);
    loop_flag = (read_16bitLE(0x0a,streamFile) == 0x472C); /* loop seems to be like this */
    channel_count = read_16bitLE(0x0e,streamFile); /* always stereo files */
    
    if (data_size + start_offset != get_streamfile_size(streamFile))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_XBOX_WVS;


    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* 
       WVS (found in Metal Arms - Glitch in the System)
*/
VGMSTREAM * init_vgmstream_ngc_wvs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag;
    int channel_count;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("wvs",filename_extension(filename))) goto fail;

    if ((read_32bitBE(0x14,streamFile)*read_32bitBE(0x00,streamFile)+0x60)
        != (get_streamfile_size(streamFile)))
        {
            goto fail;
        }
    
    loop_flag = read_32bitBE(0x10,streamFile);
    channel_count = read_32bitBE(0x00,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x60;

    if (channel_count == 1) {
        vgmstream->sample_rate = 22050;
    } else if (channel_count == 2) {
        vgmstream->sample_rate = 44100;
    }

    vgmstream->channels = channel_count;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->num_samples = (get_streamfile_size(streamFile)-start_offset)/8/channel_count*14; //(read_32bitBE(0x0C,streamFile)-start_offset)/8/channel_count*14;
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0x10,streamFile)*2)/8/channel_count*14;
        vgmstream->loop_end_sample = (read_32bitBE(0x14,streamFile)*2)/8/channel_count*14;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0x0C,streamFile);
    vgmstream->meta_type = meta_NGC_WVS;


    if (vgmstream->coding_type == coding_NGC_DSP) {
        int i,c;
        for (c=0;c<channel_count;c++) {
            for (i=0;i<16;i++) {
                vgmstream->ch[c].adpcm_coef[i] =
                    read_16bitBE(0x18+c*0x20 +i*2,streamFile);
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
