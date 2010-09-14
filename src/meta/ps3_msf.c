#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

VGMSTREAM * init_vgmstream_ps3_msf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;
    int loop_flag = 0;
    int channel_count;
    long sample_rate = 0;
    long num_samples = 0;

#ifdef VGM_USE_MPEG
    mpeg_codec_data *mpeg_data = NULL;
    coding_t mpeg_coding_type = coding_MPEG1_L3;
#endif

	uint16_t mp3ID;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("msf",filename_extension(filename))) goto fail;

    /* check header */
//    if (read_32bitBE(0x00,streamFile) != 0x58564147) /* "XVAG" */
//        goto fail;



		channel_count = 2; // read_32bitBE(0x28,streamFile);
		start_offset = 0x40; //read_32bitBE(0x4,streamFile);
		//sample_rate = read_32bitBE(0x3c,streamFile);
	  num_samples = read_32bitBE(0x1C,streamFile)/960*1152;



	// MP3s ?
	mp3ID=(uint16_t)read_16bitBE(0x40,streamFile);
	if(mp3ID==0xFFFA) {
#ifdef VGM_USE_MPEG
        long rate;
        int channels,encoding;

        mpeg_data = init_mpeg_codec_data(streamFile, start_offset, -1, -1, &mpeg_coding_type); // -1 to not check sample rate or channels
        if (!mpeg_data) goto fail;

        if (MPG123_OK != mpg123_getformat(mpeg_data->m,&rate,&channels,&encoding)) goto fail;
        channel_count = channels;
        sample_rate = rate;

#else
        // reject if no MPEG support
        goto fail;
#endif

  }

			loop_flag=read_32bitBE(0x18,streamFile);


   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->channels = channel_count;
    vgmstream->meta_type = meta_PS3_MSF;
	
	if(mp3ID==0xFFFA) {
#ifdef VGM_USE_MPEG
        /* NOTE: num_samples seems to be quite wrong for MPEG */
        vgmstream->codec_data = mpeg_data;
		vgmstream->layout_type = layout_mpeg;
		vgmstream->coding_type = mpeg_coding_type;
#else
        // reject if no MPEG support
        goto fail;
#endif
	}


	if (loop_flag) {
			vgmstream->loop_start_sample = read_32bitBE(0x18,streamFile)/960*1152;
			vgmstream->loop_end_sample = read_32bitBE(0x1C,streamFile)/960*1152;
  }


    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
		if(vgmstream->layout_type == layout_interleave) {
			file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
			if (!file) goto fail;
			for (i=0;i<channel_count;i++) {
				vgmstream->ch[i].streamfile = file;

				vgmstream->ch[i].channel_start_offset=
					vgmstream->ch[i].offset=start_offset+
					vgmstream->interleave_block_size*i;

			}
		}
#ifdef VGM_USE_MPEG
		else if(vgmstream->layout_type == layout_mpeg) {
			for (i=0;i<channel_count;i++) {
				vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,MPEG_BUFFER_SIZE);
				vgmstream->ch[i].channel_start_offset= vgmstream->ch[i].offset=start_offset;
			}

        }
#endif
        else { goto fail; }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
#ifdef VGM_USE_MPEG
    if (mpeg_data) {
        mpg123_delete(mpeg_data->m);
        free(mpeg_data);

        if (vgmstream) {
            vgmstream->codec_data = NULL;
        }
    }
#endif
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
