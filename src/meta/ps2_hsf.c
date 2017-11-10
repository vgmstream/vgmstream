#include "meta.h"
#include "../util.h"

/* HSF - Found in Lowrider (PS2) - STREAM.BIN archive */
VGMSTREAM * init_vgmstream_ps2_hsf(STREAMFILE *streamFile) 
{
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;
    size_t fileLength;
    size_t frequencyFlag;

#if 0
    off_t readOffset = 0;
    uint8_t testBuffer[0x10];
    off_t loopEndOffset;
#endif

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("hsf",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x48534600) // "HSF"
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    fileLength = get_streamfile_size(streamFile);
    frequencyFlag = read_32bitLE(0x08, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x10;
    vgmstream->channels = channel_count;
    
    if (frequencyFlag == 0x0EB3)
    {
        vgmstream->sample_rate = 44100;
    }
    else if (frequencyFlag == 0x1000)
    {
        vgmstream->sample_rate = 48000;
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = ((fileLength - 0x10) / 16 * 28) / vgmstream->channels;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x0C, streamFile);
    vgmstream->meta_type = meta_PS2_HSF;

    if (vgmstream->loop_flag)
    {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;

#if 0
        readOffset = fileLength - 0x10;

        do
        {
            readOffset -=(off_t)read_streamfile(testBuffer, readOffset, 0x10, streamFile);

            if (testBuffer[1] == 0x07)
            {
                loopEndOffset = readOffset + 0x10;
                vgmstream->loop_end_sample = ((loopEndOffset - 0x10) / 16 * 28) / vgmstream->channels;
                break;
            }

        } while (readOffset > 0);
#endif
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
