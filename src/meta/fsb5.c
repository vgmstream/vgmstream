#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* FSB5 - FMOD Studio multiplatform format */
VGMSTREAM * init_vgmstream_fsb5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t StartOffset = 0;
    off_t SampleHeaderStart = 0, DSPInfoStart = 0;
    size_t SampleHeaderLength, NameTableLength, SampleDataLength, BaseHeaderLength, StreamSize = 0;

    uint32_t NumSamples = 0, LoopStart = 0, LoopEnd = 0;
    int LoopFlag = 0, ChannelCount = 0, SampleRate = 0, CodingID;
    int TotalStreams, TargetStream = 0;
    uint32_t VorbisSetupId = 0;
    int i;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"fsb")) goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x46534235) goto fail; /* "FSB5" */

    //v0 has extra flags at 0x1c and BaseHeaderLength = 0x40?
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
    if (TargetStream > TotalStreams || TotalStreams <= 0) goto fail;

    /* find target stream header and data offset, and read all needed values for later use
     *  (reads one by one as the size of a single stream header is variable) */
    for (i = 1; i <= TotalStreams; i++) {
        off_t  DataStart = 0;
        size_t StreamHeaderLength = 0;
        uint32_t SampleMode, SampleMode2;


        /* seems ok but could use some testing against FMOD's SDK */
        SampleMode  = (uint32_t)read_32bitLE(SampleHeaderStart+0x00,streamFile);
        SampleMode2 = (uint32_t)read_32bitLE(SampleHeaderStart+0x04,streamFile);
        StreamHeaderLength += 0x08;

        /* get samples */
        NumSamples  = ((SampleMode2 >> 2) & 0x3FFFFFFF); /* bits 31..2 (30) */
        // bits 1..0 part of DataStart?

        /* get offset inside data section */
        DataStart   = ((SampleMode >> 7) & 0x0FFFFFF) << 5; /* bits 31..8 (24) * 0x20 */

        /* get channels (from tests seems correct, but multichannel isn't very common, ex. no 4ch mode?) */
        switch ((SampleMode >> 5) & 0x03) { /* bits 7..6 (2) */
            case 0:  ChannelCount = 1; break;
            case 1:  ChannelCount = 2; break;
            case 2:  ChannelCount = 6; break;/* some Dark Souls 2 MPEG; some IMA ADPCM */
            case 3:  ChannelCount = 8; break;/* some IMA ADPCM */
            default: /* other values (ex. 10ch) are specified in the extra flags, using 0 here */
                goto fail;
        }

        /* get sample rate  */
        switch ((SampleMode >> 1) & 0x0f) { /* bits 5..1 (4) */
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
            default: /* probably specified in the extra flags */
                SampleRate = 44100;
                break;
        }

        /* get extra flags */
        if (SampleMode & 0x01) { /* bit 0 (1) */
            uint32_t ExtraFlag, ExtraFlagStart, ExtraFlagType, ExtraFlagSize, ExtraFlagEnd;

            ExtraFlagStart = SampleHeaderStart+0x08;
            do {
                ExtraFlag = read_32bitLE(ExtraFlagStart,streamFile);
                ExtraFlagType = (ExtraFlag >> 25) & 0x7F; /* bits 32..26 (7) */
                ExtraFlagSize = (ExtraFlag >> 1) & 0xFFFFFF; /* bits 25..1 (24)*/
                ExtraFlagEnd  = (ExtraFlag & 0x01); /* bit 0 (1) */

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

                        /* when start is 0 seems the song repeats with no real looping (ex. Sonic Boom Fire & Ice jingles) */
                        LoopFlag = (LoopStart != 0x00);
                        break;
                    case 0x04:  /* free comment, or maybe SFX info */
                        break;
                    case 0x06:  /* XMA seek table */
                        /* no need for it */
                        break;
                    case 0x07:  /* DSP Info (Coeffs) */
                        DSPInfoStart = ExtraFlagStart + 0x04;
                        break;
                    case 0x09:  /* ATRAC9 data */
                        break;
                    case 0x0a:  /* XWMA data */
                        break;
                    case 0x0b:  /* Vorbis data */
                        VorbisSetupId = (uint32_t)read_32bitLE(ExtraFlagStart+0x04,streamFile); /* crc32? */
                        /* seek table format:
                         * 0x08: table_size (total_entries = seek_table_size / (4+4)), not counting this value; can be 0
                         * 0x0C: sample number (only some samples are saved in the table)
                         * 0x10: offset within data, pointing to a FSB vorbis block (with the 16b block size header)
                         * (xN entries)
                         */
                        break;
                    //case 0x0d:  /* Unknown value (32b), found in some XMA2 and Vorbis */
                    //    break;
                    default:
                        VGM_LOG("FSB5: unknown extra flag 0x%x at 0x%04x (size 0x%x)\n", ExtraFlagType, ExtraFlagStart, ExtraFlagSize);
                        break;
                }

                ExtraFlagStart += 0x04 + ExtraFlagSize;
                StreamHeaderLength += 0x04 + ExtraFlagSize;
            } while (ExtraFlagEnd != 0x00);
        }

        /* stream found */
        if (i == TargetStream) {
            StartOffset = BaseHeaderLength + SampleHeaderLength + NameTableLength + DataStart;

            /* get stream size from next stream or datasize if there is only one */
            if (i == TotalStreams) {
                StreamSize = SampleDataLength - DataStart;
            } else {
                uint32_t NextSampleMode  = (uint32_t)read_32bitLE(SampleHeaderStart+StreamHeaderLength+0x00,streamFile);
                StreamSize = (((NextSampleMode >> 7) & 0x00FFFFFF) << 5) - DataStart;
            }

            break;
        }

        /* continue searching */
        SampleHeaderStart += StreamHeaderLength;
    }

    /* target stream not found*/
    if (!StartOffset || !StreamSize) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ChannelCount,LoopFlag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->sample_rate = SampleRate;
    vgmstream->num_streams = TotalStreams;
    vgmstream->num_samples = NumSamples;
    if (LoopFlag) {
        vgmstream->loop_start_sample = LoopStart;
        vgmstream->loop_end_sample = LoopEnd;
    }
    vgmstream->meta_type = meta_FSB5;

    switch (CodingID) {
        case 0x00:  /* FMOD_SOUND_FORMAT_NONE */
            goto fail;

        case 0x01:  /* FMOD_SOUND_FORMAT_PCM8 */
            goto fail;

        case 0x02:  /* FMOD_SOUND_FORMAT_PCM16 */
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
                vgmstream->layout_type = layout_none;
            } else {
                vgmstream->layout_type = layout_interleave_byte;
                vgmstream->interleave_block_size = 0x02;
            }

            dsp_read_coefs_be(vgmstream,streamFile,DSPInfoStart,0x2E);
            vgmstream->coding_type = coding_NGC_DSP;
            break;

        case 0x07:  /* FMOD_SOUND_FORMAT_IMAADPCM */
            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_XBOX;
            if (vgmstream->channels > 2) /* multichannel FSB IMA (interleaved header) */
                vgmstream->coding_type = coding_FSB_IMA;
            break;

        case 0x08:  /* FMOD_SOUND_FORMAT_VAG */
            goto fail;

        case 0x09:  /* FMOD_SOUND_FORMAT_HEVAG */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            vgmstream->coding_type = coding_HEVAG;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0A: {/* FMOD_SOUND_FORMAT_XMA */
            uint8_t buf[100];
            int bytes, block_size, block_count;

            block_size = 0x10000; /* XACT default */
            block_count = StreamSize / block_size + (StreamSize % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 100, vgmstream->num_samples, StreamSize, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, StartOffset,StreamSize);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0B: {/* FMOD_SOUND_FORMAT_MPEG */
            mpeg_codec_data *mpeg_data = NULL;
            coding_t mpeg_coding_type;
            int fsb_padding = 0;

            fsb_padding = vgmstream->channels > 2 ? 16 : 4; /* observed default */

            mpeg_data = init_mpeg_codec_data_interleaved(streamFile, StartOffset, &mpeg_coding_type, vgmstream->channels, 0, fsb_padding);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->coding_type = mpeg_coding_type;
            vgmstream->layout_type = layout_mpeg;

            vgmstream->interleave_block_size = mpeg_data->current_frame_size + mpeg_data->current_padding;
            //mpeg_set_error_logging(mpeg_data, 0); /* should not be needed anymore with the interleave decoder */
            break;
        }
#endif
        case 0x0C:  /* FMOD_SOUND_FORMAT_CELT */
            goto fail;

        case 0x0D:  /* FMOD_SOUND_FORMAT_AT9 */
            goto fail;

        case 0x0E:  /* FMOD_SOUND_FORMAT_XWMA */
            goto fail;

#ifdef VGM_USE_VORBIS
        case 0x0F: {/* FMOD_SOUND_FORMAT_VORBIS */
            vgmstream->codec_data = init_fsb_vorbis_codec_data(streamFile, StartOffset, vgmstream->channels, vgmstream->sample_rate,VorbisSetupId);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_fsb_vorbis;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        case 0x10:  /* FMOD_SOUND_FORMAT_FADPCM */
            goto fail;

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,StartOffset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
