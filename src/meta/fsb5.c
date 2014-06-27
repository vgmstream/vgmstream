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
    int DSPInfoStart;

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
