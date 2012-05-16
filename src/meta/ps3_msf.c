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
	size_t	fileLength;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("msf",filename_extension(filename))) goto fail;


    if (read_8bit(0x0,streamFile) != 0x4D) goto fail;	/* M */
    if (read_8bit(0x1,streamFile) != 0x53) goto fail;	/* S */
    if (read_8bit(0x2,streamFile) != 0x46) goto fail; /* F */

    fileLength = get_streamfile_size(streamFile);

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

   /* Sample rate hack for strange files that don't have a specified frequency */
	if (read_32bitBE(0x10,streamFile)==0x00000000)
		vgmstream->sample_rate = 48000;
	else
		vgmstream->sample_rate = read_32bitBE(0x10,streamFile);

    start_offset = 0x40;

    switch (codec_id) {
        case 0x0: /* PCM (Big Endian) */
            {
                vgmstream->coding_type = coding_PCM16BE;
                vgmstream->num_samples = read_32bitBE(0x0C,streamFile)/2/channel_count;
                
				if (loop_flag){
                    vgmstream->loop_start_sample = loop_start/2/channel_count;
                    vgmstream->loop_end_sample = loop_end/2/channel_count;
                }
                
                if (channel_count == 1)
                {
                  vgmstream->layout_type = layout_none;
                }
                else if (channel_count > 1)
                {
                  vgmstream->layout_type = layout_interleave;
                  vgmstream->interleave_block_size = 2;
                }
            }
            break;
        case 0x3: /* PSx ADPCM */
            {
                vgmstream->coding_type = coding_PSX;
                vgmstream->num_samples = read_32bitBE(0x0C,streamFile)*28/16/channel_count;

				if (vgmstream->num_samples == 0xFFFFFFFF)
				{
					vgmstream->num_samples = (fileLength - start_offset)*28/16/channel_count;
				}

				if (loop_flag)
				{
                    vgmstream->loop_start_sample = loop_start*28/16/channel_count;
                    vgmstream->loop_end_sample = loop_end*28/16/channel_count;
                }
                
                if (channel_count == 1)
                {
                  vgmstream->layout_type = layout_none;
                }
                else if (channel_count > 1)
                {
                  vgmstream->layout_type = layout_interleave;
                  vgmstream->interleave_block_size = 0x10; // read_32bitBE(0x14,streamFile);
                }
            }
            break;
#ifdef VGM_USE_MPEG
        case 0x7: /* MPEG */
            {
                mpeg_codec_data *mpeg_data = NULL;
                struct mpg123_frameinfo mi;
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, start_offset, vgmstream->sample_rate, vgmstream->channels, &ct, NULL, NULL);
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
