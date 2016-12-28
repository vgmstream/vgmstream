#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* FSB5 header */
VGMSTREAM * init_vgmstream_fsb5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    off_t StartOffset;
    
    int LoopFlag = 0;
    int32_t LoopStart, LoopEnd;
    
    int NumSamples;
    int ChannelCount;
    int SampleRate;
    int DSPInfoStart = 0;

    int SampleHeaderStart, SampleHeaderLength, NameTableLength, SampleDataLength, CodingID, SampleMode;
    int ExtraFlag, ExtraFlagStart, ExtraFlagType, ExtraFlagSize, ExtraFlagEnd;
    
    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x46534235) goto fail; /* "FSB5" */
    if (read_32bitLE(0x04,streamFile) != 0x01) goto fail; /* Version ID */
    if (read_32bitLE(0x08,streamFile) != 0x01) goto fail; /* Number of Sample Files */

    SampleHeaderStart = 0x3C;
    SampleHeaderLength = read_32bitLE(0x0C,streamFile);
    NameTableLength = read_32bitLE(0x10,streamFile);
    SampleDataLength = read_32bitLE(0x14,streamFile);
    CodingID = read_32bitLE(0x18,streamFile);

    if ((SampleHeaderLength + NameTableLength + SampleDataLength + 0x3C) != get_streamfile_size(streamFile)) goto fail;

    StartOffset = SampleHeaderLength + NameTableLength + 0x3C;
    SampleMode = read_32bitLE(SampleHeaderStart+0x00,streamFile);
    
    if (SampleMode&0x02)
    {
      SampleRate = 48000;
    } else {
      SampleRate = 44100;
    }

    if (SampleMode&0x20)
    {
      ChannelCount = 2;
    } else {
      ChannelCount = 1;
    }
    
    ExtraFlagStart = SampleHeaderStart+0x08;

    if (SampleMode&0x01)
    {
      do
      {
        ExtraFlag = read_32bitLE(ExtraFlagStart,streamFile);
        ExtraFlagType = (ExtraFlag>>25)&0x7F;
        ExtraFlagSize = (ExtraFlag>>1)&0xFFFFFF;
        ExtraFlagEnd = (ExtraFlag&0x01);

        switch(ExtraFlagType)
        {
        case 0x02: /* Sample Rate Info */
          {
            SampleRate = read_32bitLE(ExtraFlagStart+0x04,streamFile);
          }
          break;
        
        case 0x03: /* Loop Info */
          {
            LoopStart = read_32bitLE(ExtraFlagStart+0x04,streamFile);
            if (LoopStart != 0x00) {
              LoopFlag = 1;
              LoopEnd = read_32bitLE(ExtraFlagStart+0x08,streamFile);
            }
          }
          break;

        case 0x07: /* DSP Info (Coeffs), only used if coding is DSP??? */
          {
            DSPInfoStart = ExtraFlagStart+0x04;
          }
          break;

        }
        ExtraFlagStart+=ExtraFlagSize+0x04;
      }
      while (ExtraFlagEnd != 0x00);
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ChannelCount,LoopFlag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	  vgmstream->channels = ChannelCount;
		vgmstream->sample_rate = SampleRate;


    switch (CodingID)
    {
    case 0x00: /* FMOD_SOUND_FORMAT_NONE */
      {
        goto fail;
      }
      break;
    case 0x01: /* FMOD_SOUND_FORMAT_PCM8 */
      {
        goto fail;
      }
      break;

    case 0x02: /* FMOD_SOUND_FORMAT_PCM16 */
      {

        NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/4;

        if (ChannelCount == 1)
        {
          vgmstream->layout_type = layout_none;
        } else {
          vgmstream->layout_type = layout_interleave;
          vgmstream->interleave_block_size = 0x02;
        }


        vgmstream->coding_type = coding_PCM16LE;
      }
      break;

    case 0x03:/* FMOD_SOUND_FORMAT_PCM24 */
      {
        goto fail;
      }
      break;

    case 0x04: /* FMOD_SOUND_FORMAT_PCM32 */
      {
        goto fail;
      }
      break;

    case 0x05: /* FMOD_SOUND_FORMAT_PCMFLOAT */
      {
        goto fail;
      }
      break;

    case 0x06: /* FMOD_SOUND_FORMAT_GCADPCM */
      {
        if (ChannelCount == 1)
        {
          NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/4;
          vgmstream->layout_type = layout_none;
        } else {
          NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/(2*ChannelCount);
          vgmstream->layout_type = layout_interleave_byte;
          vgmstream->interleave_block_size = 0x02;
        }

        vgmstream->coding_type = coding_NGC_DSP;

        /* DSP Coeffs */
        {
          int c,i;
          for (c=0;c<ChannelCount;c++) {
            for (i=0;i<16;i++)
            {
              vgmstream->ch[c].adpcm_coef[i] = read_16bitBE(DSPInfoStart + c*0x2E + i*2,streamFile);
            }
          }
        }
      }
      break;

    case 0x07: /* FMOD_SOUND_FORMAT_IMAADPCM */
      {

        NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/4;
        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_XBOX;

      }
      break;

    case 0x08: /* FMOD_SOUND_FORMAT_VAG */
      {
        goto fail;
      }
      break;

    case 0x09: /* FMOD_SOUND_FORMAT_HEVAG */
      {
        goto fail;
      }
      break;

    case 0x0A: /* FMOD_SOUND_FORMAT_XMA */
      {
        goto fail;
      }
      break;

    case 0x0B: /* FMOD_SOUND_FORMAT_MPEG */
      {
        NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/2/ChannelCount;

        #ifdef VGM_USE_MPEG
            {
                mpeg_codec_data *mpeg_data = NULL;
                struct mpg123_frameinfo mi;
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, StartOffset, vgmstream->sample_rate, vgmstream->channels, &ct, NULL, NULL);
                if (!mpeg_data) goto fail;
                vgmstream->codec_data = mpeg_data;

                if (MPG123_OK != mpg123_info(mpeg_data->m, &mi)) goto fail;

                vgmstream->coding_type = ct;
                vgmstream->layout_type = layout_mpeg;
                if (mi.vbr != MPG123_CBR) goto fail;
                vgmstream->interleave_block_size = 0;
            }
            break;
#endif
      }
      break;

    case 0x0C: /* FMOD_SOUND_FORMAT_CELT */
      {
        goto fail;
      }
      break;

    case 0x0D: /* FMOD_SOUND_FORMAT_AT9 */
      {
        goto fail;
      }
      break;

    case 0x0E: /* FMOD_SOUND_FORMAT_XWMA */
      {
        goto fail;
      }
      break;

    case 0x0F: /* FMOD_SOUND_FORMAT_VORBIS */
      {
        goto fail;
      }
      break;
        default:
            goto fail;
    }
    
    vgmstream->num_samples = NumSamples;
    vgmstream->meta_type = meta_FSB5;    

    if (LoopFlag)
    {
      vgmstream->loop_start_sample = LoopStart;
      vgmstream->loop_end_sample = LoopEnd;
    }

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<ChannelCount;i++) {
            vgmstream->ch[i].streamfile = file;

            
            if (vgmstream->coding_type == coding_XBOX) {
                /* xbox interleaving is a little odd */
                vgmstream->ch[i].channel_start_offset=StartOffset;
            } else {
                vgmstream->ch[i].channel_start_offset=
                    StartOffset+vgmstream->interleave_block_size*i;
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

#if 0
// FSB5 MPEG
VGMSTREAM * init_vgmstream_fsb5_mpeg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    int channel_count, channels, loop_flag, fsb_mainheader_len, fsb_subheader_len, FSBFlag, rate;
    long sample_rate = 0, num_samples = 0;
    uint16_t mp3ID;

#ifdef VGM_USE_MPEG
    mpeg_codec_data *mpeg_data = NULL;
    coding_t mpeg_coding_type = coding_MPEG1_L3;
#endif

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("fsb",filename_extension(filename))) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) == 0x46534235) /* "FSB5" */
    {
        fsb_mainheader_len = 0x3C;
    }
    else
    {
        goto fail;
    }

    //fsb_subheader_len = read_16bitLE(fsb_mainheader_len,streamFile);

    /* "Check if the FSB is used as conatiner or as single file" */
    if (read_32bitBE(0x04,streamFile) != 0x01000000)
        goto fail;

#if 0
    /* Check channel count, multi-channel not supported and will be refused */
    if ((read_16bitLE(0x6E,streamFile) != 0x2) &&
       (read_16bitLE(0x6E,streamFile) != 0x1))
        goto fail;
#endif

    start_offset = fsb_mainheader_len+fsb_subheader_len+0x10;

    /* Check the MPEG Sync Header */
    mp3ID = read_16bitBE(start_offset,streamFile);
    if ((mp3ID&0xFFE0) != 0xFFE0)
        goto fail;

    channel_count = read_16bitLE(fsb_mainheader_len+0x3E,streamFile);
    if (channel_count != 1 && channel_count != 2)
        goto fail;

    FSBFlag = read_32bitLE(fsb_mainheader_len+0x30,streamFile);
    if (FSBFlag&0x2 || FSBFlag&0x4 || FSBFlag&0x6)
      loop_flag = 1;

    num_samples = (read_32bitLE(fsb_mainheader_len+0x2C,streamFile));

#ifdef VGM_USE_MPEG
        mpeg_data = init_mpeg_codec_data(streamFile, start_offset, -1, -1, &mpeg_coding_type, &rate, &channels); // -1 to not check sample rate or channels
        if (!mpeg_data) goto fail;

        //channel_count = channels;
        sample_rate = rate;

#else
        // reject if no MPEG support
        goto fail;
#endif

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->channels = channel_count;

    /* Still WIP */
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(fsb_mainheader_len+0x28,streamFile);
       vgmstream->loop_end_sample = read_32bitLE(fsb_mainheader_len+0x2C,streamFile);
    }
    vgmstream->meta_type = meta_FSB_MPEG;

#ifdef VGM_USE_MPEG
        /* NOTE: num_samples seems to be quite wrong for MPEG */
        vgmstream->codec_data = mpeg_data;
        vgmstream->layout_type = layout_mpeg;
        vgmstream->coding_type = mpeg_coding_type;
#else
        // reject if no MPEG support
        goto fail;
#endif


#if 0
    if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x18,streamFile)/960*1152;
            vgmstream->loop_end_sample = read_32bitBE(0x1C,streamFile)/960*1152;
  }
#endif

    /* open the file for reading */
    {
    int i;
      STREAMFILE * file;
        if(vgmstream->layout_type == layout_interleave)
        {
          file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
                if (!file) goto fail;
                    for (i=0;i<channel_count;i++)
              {
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
#endif
