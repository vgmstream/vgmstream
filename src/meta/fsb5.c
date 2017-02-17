#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* FSB5 header */
VGMSTREAM * init_vgmstream_fsb5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t StartOffset;
    
    int LoopFlag = 0;
    int32_t LoopStart, LoopEnd;
    
    int NumSamples;
    int ChannelCount;
    int SampleRate;
    int DSPInfoStart = 0;

    int SampleHeaderStart, SampleHeaderLength, NameTableLength, SampleDataLength, CodingID, SampleMode;
    int ExtraFlag, ExtraFlagStart, ExtraFlagType, ExtraFlagSize, ExtraFlagEnd;
    int freq_mode, ch_mode;
    
    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"fsb")) goto fail;

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
    
    /* get sample rate  */
    freq_mode = (SampleMode >> 1) & 0x0f; /* bits 5..1 */
    switch (freq_mode) {
        case 0:  SampleRate = 4000;  break; //???
        case 1:  SampleRate = 8000;  break;
        case 2:  SampleRate = 11000; break;
        case 3:  SampleRate = 11025; break;
        case 4:  SampleRate = 16000; break;
        case 5:  SampleRate = 22050; break;
        case 6:  SampleRate = 24000; break;
        case 7:  SampleRate = 32000; break;
        case 8:  SampleRate = 44100; break;
        case 9:  SampleRate = 48000; break;
        case 10: SampleRate = 96000; break; //???
        default:
            SampleRate = 44100;
            //goto fail; /* probably better? */
            break;
    }

    /* get channels (from tests seems correct, but multichannel isn't very common, ex. no 4ch mode?) */
    ch_mode = (SampleMode >> 5) & 0x03; /* bits 7..6 (maybe 8 too?) */
    switch (ch_mode) {
        case 0:  ChannelCount = 1; break;
        case 1:  ChannelCount = 2; break;
        case 2:  ChannelCount = 6; break;/* some Dark Souls 2 MPEG; some IMA ADPCM */
        case 3:  ChannelCount = 8; break;/* some IMA ADPCM */
        /* other values (ex. 10ch) seem specified in the extra flags */
        default:
            goto fail;
    }

    /* get extra flags */
    ExtraFlagStart = SampleHeaderStart+0x08;
    if (SampleMode&0x01) /* bit 0 */
    {
      do
      {
        ExtraFlag = read_32bitLE(ExtraFlagStart,streamFile);
        ExtraFlagType = (ExtraFlag>>25)&0x7F;
        ExtraFlagSize = (ExtraFlag>>1)&0xFFFFFF;
        ExtraFlagEnd = (ExtraFlag&0x01);

        switch(ExtraFlagType)
        {
        case 0x01: /* Channel Info */
          {
              ChannelCount = read_8bit(ExtraFlagStart+0x04,streamFile);
          }
          break;
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

        dsp_read_coefs_be(vgmstream,streamFile,DSPInfoStart,0x2E);
      }
      break;

    case 0x07: /* FMOD_SOUND_FORMAT_IMAADPCM */
      {
        NumSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile)/4;
        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_XBOX;
        if (vgmstream->channels > 2) /* multichannel FSB IMA (interleaved header) */
            vgmstream->coding_type = coding_FSB_IMA;
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
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, StartOffset, vgmstream->sample_rate, vgmstream->channels, &ct, NULL, NULL);
                if (!mpeg_data) goto fail;
                vgmstream->codec_data = mpeg_data;

                vgmstream->coding_type = ct;
                vgmstream->layout_type = layout_mpeg;
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

    if (!vgmstream_open_stream(vgmstream,streamFile,StartOffset))
        goto fail;

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
