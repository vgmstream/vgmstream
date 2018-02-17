#include "meta.h"
#include "../coding/coding.h"

/* XWAV - renamed WAV with XBOX-IMA
 * (could be parsed as RIFF/.lwav but has a custom loop chunk and multichannel) */
VGMSTREAM * init_vgmstream_xbox_xwav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset;

    /* check extension */
    if (!check_extensions(streamFile,"xwav"))
        goto fail;

    /* check for headers */
    if(!((read_32bitBE(0x00,streamFile) == 0x52494646) && /* "RIFF" */
         (read_32bitBE(0x08,streamFile) == 0x57415645) && /* "WAVE" */
         (read_32bitBE(0x0C,streamFile) == 0x666D7420) && /* "fmt " */
         (read_16bitLE(0x14,streamFile) == 0x0069))) /* codec */
         goto fail;

    /* loop chunk found on Koei/Omega Force games [Crimson Sea, Dynasty Warriors 5] */
    loop_flag = (read_32bitBE(0x28,streamFile) == 0x77736D70); /* "wsmp" */
    channel_count = read_16bitLE(0x16,streamFile);

    /* search for "data" */
    start_offset = 0x1C;
    do {
        if (read_32bitBE(start_offset,streamFile)==0x64617461)
            break;
        start_offset += 0x04;
    } while (start_offset < (off_t)get_streamfile_size(streamFile));

    if (start_offset >= (off_t)get_streamfile_size(streamFile))
        goto fail;
    start_offset += 0x04;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = xbox_ima_bytes_to_samples(read_32bitLE(start_offset,streamFile), vgmstream->channels);
    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x4C,streamFile);
        vgmstream->loop_end_sample = vgmstream->loop_start_sample + read_32bitLE(0x50,streamFile);
    }

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_XBOX_RIFF;

    //if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
    //    goto fail;

    //custom init
    {
        int i, ch;
        char filename[PATH_LIMIT];
        streamFile->get_name(streamFile,filename,sizeof(filename));

        if (channel_count > 2) { /* multichannel interleaved init */
            for (i=0, ch=0;i<channel_count;i++,ch++) {
                if ((ch&2) && (i!=0)) {
                    ch = 0;
                    start_offset += 0x24*2;
                }

                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,0x24);
                vgmstream->ch[i].offset = start_offset + 0x04;

                if (!vgmstream->ch[i].streamfile) goto fail;
            }
        }
        else {
            for (i=0; i < channel_count; i++) {
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,0x24);
                vgmstream->ch[i].offset = start_offset + 0x04;

                if (!vgmstream->ch[i].streamfile) goto fail;
            }
        }
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
