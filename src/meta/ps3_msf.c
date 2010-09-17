#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* MSF header */
VGMSTREAM * init_vgmstream_ps3_msf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int32_t loop_start, loop_end;
    int loop_flag = 0;
  	int channel_count;
    int codec_id;

//    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
//    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("msf",filename_extension(filename))) goto fail;

    /* MSFC */
    if (read_32bitBE(0x0,streamFile) != 0x4D534643) goto fail;

    loop_flag = (read_32bitBE(0x18,streamFile) != 0xFFFFFFFF);
    if (loop_flag)
    {
      loop_start = read_32bitBE(0x18,streamFile);
      loop_end = read_32bitBE(0x0C,streamFile);
    }

    channel_count = read_32bitBE(0x8,streamFile);
    codec_id = read_32bitBE(0x4,streamFile);
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitBE(0x10,streamFile);
    start_offset = 0x40;

    switch (codec_id) {
        case 0x3:
            {
                vgmstream->coding_type = coding_PSX;
                vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/16/channel_count*28;
                if (loop_flag) {
                    vgmstream->loop_start_sample = read_32bitBE(0x18,streamFile)/16/channel_count*28;
                    vgmstream->loop_end_sample = read_32bitBE(0x0C,streamFile)/16/channel_count*28;
                }
                
                if (channel_count == 1)
                {
                  vgmstream->layout_type = layout_none;
                }
                else if (channel_count > 1)
                {
                  vgmstream->layout_type = layout_interleave;
                  vgmstream->interleave_block_size = read_32bitBE(0x14,streamFile); // Not sure
                }
            }
            break;
#ifdef VGM_USE_MPEG
        case 0x7:
            /* MPEG */
            {
                mpeg_codec_data *mpeg_data = NULL;
                struct mpg123_frameinfo mi;
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, start_offset, vgmstream->sample_rate, vgmstream->channels, &ct);
                if (!mpeg_data) goto fail;
                vgmstream->codec_data = mpeg_data;

                if (MPG123_OK != mpg123_info(mpeg_data->m, &mi)) goto fail;

                vgmstream->coding_type = ct;
                vgmstream->layout_type = layout_mpeg;
                if (mi.vbr != MPG123_CBR) goto fail;
                vgmstream->num_samples = mpeg_bytes_to_samples(read_32bitBE(0xC,streamFile), &mi);
                vgmstream->num_samples -= vgmstream->num_samples%576;
                if (loop_flag) {
                    vgmstream->loop_start_sample = mpeg_bytes_to_samples(loop_start, &mi);
                    vgmstream->loop_start_sample -= vgmstream->loop_start_sample%576;
                    vgmstream->loop_end_sample = mpeg_bytes_to_samples(loop_end, &mi);
                    vgmstream->loop_end_sample -= vgmstream->loop_end_sample%576;
                }
                vgmstream->interleave_block_size = 0;
            }
            break;
#endif
        default:
            goto fail;
    }

    vgmstream->meta_type = meta_PS3_MSF;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
