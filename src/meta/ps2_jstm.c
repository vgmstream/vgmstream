#include "meta.h"
#include "../util.h"

/* JSTM (.STM (renamed .JSTM) from Tantei Jinguji Saburo - Kind of Blue) */
VGMSTREAM * init_vgmstream_ps2_jstm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0x20;
    int loop_flag;
    int channel_count;
    char filename[260];

    /* check extension */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("stm",filename_extension(filename)) &&
        strcasecmp("jstm",filename_extension(filename))) goto fail;

    /* check header (JSTM) */
    if (read_32bitBE(0x0,streamFile) != 0x4A53544D) goto fail;

    loop_flag = (read_32bitLE(0x14,streamFile) != 0);
    channel_count = read_16bitLE(0x4,streamFile);
    
    // hmm, don't know what 6 is, one is probably bytes per sample and the
    // other is channels, but who can say?
    if (channel_count != read_16bitLE(0x6,streamFile)) goto fail;

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the statistics vitale */
    vgmstream->sample_rate = read_32bitLE(0x8,streamFile);
    vgmstream->coding_type = coding_PCM16LE_XOR_int;
    vgmstream->num_samples = read_32bitLE(0xC,streamFile)/2/channel_count;
    vgmstream->layout_type = layout_none;

    vgmstream->meta_type = meta_PS2_JSTM;

    if (loop_flag) {
        vgmstream->loop_start_sample=read_32bitLE(0x14,streamFile)/2/channel_count;
        vgmstream->loop_end_sample=vgmstream->num_samples;
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;

        for (i=0; i < channel_count; i++) {
            vgmstream->ch[i].streamfile = file;
            vgmstream->ch[i].channel_start_offset = 
                vgmstream->ch[i].offset = start_offset + 2*i;
            vgmstream->ch[i].key_xor = 0x5A5A;
        }
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
