#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* FSB5 header */
VGMSTREAM * init_vgmstream_fsb5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t StartOffset = 0;
    off_t SampleHeaderStart = 0, DSPInfoStart = 0;
    size_t SampleHeaderLength, NameTableLength, SampleDataLength, BaseHeaderLength;

    uint32_t BaseSamples = 0, LoopStart = 0, LoopEnd = 0, NumSamples = 0;
    int LoopFlag = 0, ChannelCount = 0, SampleRate = 0, CodingID;
    int TotalStreams, TargetStream = 0;
    int i;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"fsb")) goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x46534235) goto fail; /* "FSB5" */

    //v0 has extra flags at 0x1c and SampleHeaderStart = 0x40?
    if (read_32bitLE(0x04,streamFile) != 0x01) goto fail; /* Version ID */

    TotalStreams       = read_32bitLE(0x08,streamFile);
    SampleHeaderLength = read_32bitLE(0x0C,streamFile);
    NameTableLength    = read_32bitLE(0x10,streamFile);
    SampleDataLength   = read_32bitLE(0x14,streamFile);
    CodingID = read_32bitLE(0x18,streamFile);
    /* 0x1c (8): zero,  0x24 (16): hash,  0x34 (8): unk  */
    BaseHeaderLength = 0x3C;

    SampleHeaderStart = BaseHeaderLength;

    if ((SampleHeaderLength + NameTableLength + SampleDataLength + 0x3C) != get_streamfile_size(streamFile)) goto fail;
    if (TargetStream == 0) TargetStream = 1; /* default to 1 */
    if (TargetStream > TotalStreams || TotalStreams < 0) goto fail;

    /* find target stream header and data offset and read all needed values for later use
     *  (reads one by one as the size of a single stream header is variable) */
    for (i = 0; i < TotalStreams; i++) {
        off_t  DataStart = 0;
        size_t StreamHeaderLength = 0;
        uint32_t SampleMode;

        SampleMode  = read_32bitLE(SampleHeaderStart+0x00,streamFile);
        BaseSamples = read_32bitLE(SampleHeaderStart+0x04,streamFile);
        StreamHeaderLength += 0x08;

        /* get global offset */
        DataStart = (SampleMode >> 7) * 0x20;

        /* get sample rate  */
        switch ((SampleMode >> 1) & 0x0f) { /* bits 5..1 */
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
                break; /* probably specified in the extra flags */
        }

        /* get channels (from tests seems correct, but multichannel isn't very common, ex. no 4ch mode?) */
        switch ((SampleMode >> 5) & 0x03) { /* bits 7..6 */
            case 0:  ChannelCount = 1; break;
            case 1:  ChannelCount = 2; break;
            case 2:  ChannelCount = 6; break;/* some Dark Souls 2 MPEG; some IMA ADPCM */
            case 3:  ChannelCount = 8; break;/* some IMA ADPCM */
            default: /* other values (ex. 10ch) seem specified in the extra flags */
                goto fail;
        }

        /* get extra flags */
        if (SampleMode&0x01) { /* bit 0 */
            uint32_t ExtraFlag, ExtraFlagStart, ExtraFlagType, ExtraFlagSize, ExtraFlagEnd;

            ExtraFlagStart = SampleHeaderStart+0x08;
            do {
                ExtraFlag = read_32bitLE(ExtraFlagStart,streamFile);
                ExtraFlagType = (ExtraFlag>>25)&0x7F;
                ExtraFlagSize = (ExtraFlag>>1)&0xFFFFFF;
                ExtraFlagEnd = (ExtraFlag&0x01);

                switch(ExtraFlagType) {
                    case 0x01:  /* Channel Info */
                        ChannelCount = read_8bit(ExtraFlagStart+0x04,streamFile);
                        break;
                    case 0x02:  /* Sample Rate Info */
                        SampleRate = read_32bitLE(ExtraFlagStart+0x04,streamFile);
                        break;
                    case 0x03:  /* Loop Info */
                        LoopStart = read_32bitLE(ExtraFlagStart+0x04,streamFile);
                        if (ExtraFlagSize > 0x04) /* probably no needed */
                            LoopEnd = read_32bitLE(ExtraFlagStart+0x08,streamFile);

                        /* when start is 0 seems the song reoeats with no real looping (ex. Sonic Boom Fire & Ice jingles) */
                        LoopFlag = (LoopStart != 0x00);
                        break;
                    case 0x07:  /* DSP Info (Coeffs), only used if coding is DSP??? */
                        DSPInfoStart = ExtraFlagStart + 0x04;
                        break;
                    default:
                        VGM_LOG("FSB5: unknown extra flag %i at 0x%04x\n", ExtraFlagType, ExtraFlagStart);
                        break;
                }

                ExtraFlagStart += 0x04 + ExtraFlagSize;
                StreamHeaderLength += 0x04 + ExtraFlagSize;
            } while (ExtraFlagEnd != 0x00);
        }

        /* stream found */
        if (i == TotalStreams-1) {
            StartOffset = BaseHeaderLength + SampleHeaderLength + NameTableLength + DataStart;
            break;
        }

        /* continue searching */
        SampleHeaderStart += StreamHeaderLength;
    }
    /* target stream not found*/
    if (!StartOffset) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ChannelCount,LoopFlag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = ChannelCount;
    vgmstream->sample_rate = SampleRate;
    vgmstream->num_streams = TotalStreams;
    vgmstream->meta_type = meta_FSB5;

    switch (CodingID) {
        case 0x00:  /* FMOD_SOUND_FORMAT_NONE */
            goto fail;

        case 0x01:  /* FMOD_SOUND_FORMAT_PCM8 */
            goto fail;

        case 0x02:  /* FMOD_SOUND_FORMAT_PCM16 */
            NumSamples = BaseSamples / 4;
            if (ChannelCount == 1) {
                vgmstream->layout_type = layout_none;
            } else {
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x02;
            }

            vgmstream->coding_type = coding_PCM16LE;
            break;

        case 0x03:  /* FMOD_SOUND_FORMAT_PCM24 */
            goto fail;

        case 0x04:  /* FMOD_SOUND_FORMAT_PCM32 */
            goto fail;

        case 0x05:  /* FMOD_SOUND_FORMAT_PCMFLOAT */
            goto fail;

        case 0x06:  /* FMOD_SOUND_FORMAT_GCADPCM */
            if (ChannelCount == 1) {
                NumSamples = BaseSamples / 4;
                vgmstream->layout_type = layout_none;
            } else {
                NumSamples = BaseSamples / (2*ChannelCount);
                vgmstream->layout_type = layout_interleave_byte;
                vgmstream->interleave_block_size = 0x02;
            }

            dsp_read_coefs_be(vgmstream,streamFile,DSPInfoStart,0x2E);
            vgmstream->coding_type = coding_NGC_DSP;
            break;

        case 0x07:  /* FMOD_SOUND_FORMAT_IMAADPCM */
            NumSamples = BaseSamples / 4;
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_XBOX;
            if (vgmstream->channels > 2) /* multichannel FSB IMA (interleaved header) */
                vgmstream->coding_type = coding_FSB_IMA;
            break;

        case 0x08:  /* FMOD_SOUND_FORMAT_VAG */
            goto fail;

        case 0x09:  /* FMOD_SOUND_FORMAT_HEVAG */
            goto fail;

        case 0x0A:  /* FMOD_SOUND_FORMAT_XMA */
            goto fail;

#ifdef VGM_USE_MPEG
        case 0x0B: {/* FMOD_SOUND_FORMAT_MPEG */
            mpeg_codec_data *mpeg_data = NULL;
            coding_t mpeg_coding_type;

            NumSamples = BaseSamples / 2 / ChannelCount;

#if 0
            int fsb_padding = vgmstream->channels > 2 ? 16 : 0;//todo fix

            mpeg_data = init_mpeg_codec_data_interleaved(streamFile, StartOffset, &mpeg_coding_type, vgmstream->channels, 0, fsb_padding);
            if (!mpeg_data) goto fail;

            vgmstream->interleave_block_size = mpeg_data->current_frame_size + mpeg_data->current_padding;
            if (vgmstream->channels > 2) vgmstream->loop_flag = 0;//todo not implemented yet
#endif

            if (vgmstream->channels > 2)
                goto fail; /* no multichannel for now */

            mpeg_data = init_mpeg_codec_data(streamFile, StartOffset, &mpeg_coding_type, vgmstream->channels);
            if (!mpeg_data) goto fail;

            vgmstream->codec_data = mpeg_data;
            vgmstream->coding_type = mpeg_coding_type;
            vgmstream->layout_type = layout_mpeg;

            mpeg_set_error_logging(mpeg_data, 0);
            break;
        }
#endif
        case 0x0C: /* FMOD_SOUND_FORMAT_CELT */
            goto fail;

        case 0x0D: /* FMOD_SOUND_FORMAT_AT9 */
            goto fail;

        case 0x0E: /* FMOD_SOUND_FORMAT_XWMA */
            goto fail;

        case 0x0F: /* FMOD_SOUND_FORMAT_VORBIS */
            goto fail;

        default:
            goto fail;
    }

    vgmstream->num_samples = NumSamples;
    if (LoopFlag) {
        vgmstream->loop_start_sample = LoopStart;
        vgmstream->loop_end_sample = LoopEnd;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,StartOffset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
