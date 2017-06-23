#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* MSF - Sony's PS3 SDK format (MultiStream File) */
VGMSTREAM * init_vgmstream_ps3_msf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset = 0;
    uint32_t data_size, loop_start = 0, loop_end = 0;
  	uint32_t id, codec_id, flags;
    int loop_flag = 0, channel_count;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"msf,at3")) goto fail; /* .at3: Silent Hill HD Collection */

    /* "WMSF" variation with a mini header over the MSFC header, same extension */
    if (read_32bitBE(0x00,streamFile) == 0x574D5346) {
        header_offset = 0x10;
    }
    start_offset = header_offset+0x40; /* MSF header is always 0x40 */

    /* check header "MSF" + version-char
     *  usually "MSF\0\1", "MSF\0\2", "MSF0"(\3\0), "MSF5"(\3\5), "MSFC"(\4\3) (last/common version) */
    id = read_32bitBE(header_offset+0x00,streamFile);
    if ((id & 0xffffff00) != 0x4D534600) goto fail;


    codec_id = read_32bitBE(header_offset+0x04,streamFile);
    channel_count = read_32bitBE(header_offset+0x08,streamFile);
    data_size = read_32bitBE(header_offset+0x0C,streamFile); /* without header */
    if (data_size == 0xFFFFFFFF) /* unneeded? */
        data_size = get_streamfile_size(streamFile) - start_offset;

    /* byte flags, not in MSFv1 or v2
     *  0x01/02/04/08: loop marker 0/1/2/3
     *  0x10: resample options (force 44/48khz)
     *  0x20: VBR MP3
     *  0x40: joint stereo MP3 (apparently interleaved stereo for other formats)
     *  0x80+: (none/reserved) */
    flags = read_32bitBE(header_offset+0x14,streamFile);
    /* sometimes loop_start/end is set with flag 0x10, but from tests it only loops if 0x01/02 is set
     * 0x10 often goes with 0x01 but not always (Castlevania HoD); Malicious PS3 uses flag 0x2 instead */
    loop_flag = flags != 0xffffffff && ((flags & 0x01) || (flags & 0x02));

    /* loop markers (marker N @ 0x18 + N*(4+4), but in practice only marker 0 is used) */
    if (loop_flag) {
        loop_start = read_32bitBE(header_offset+0x18,streamFile);
        loop_end = read_32bitBE(header_offset+0x1C,streamFile); /* loop duration */
        loop_end = loop_start + loop_end; /* usually equals data_size but not always */
        if (loop_end > data_size)/* not seen */
            loop_end = data_size;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* Sample rate hack for strange MSFv1 files that don't have a specified frequency */
    vgmstream->sample_rate = read_32bitBE(header_offset+0x10,streamFile);
	if (vgmstream->sample_rate == 0x00000000) /* PS ADPCM only? */
		vgmstream->sample_rate = 48000;

    vgmstream->meta_type = meta_PS3_MSF;

    switch (codec_id) {
        case 0x0:   /* PCM (Big Endian) */
        case 0x1: { /* PCM (Little Endian) */
            vgmstream->coding_type = codec_id==0 ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 2;

            vgmstream->num_samples = data_size/2/channel_count;
            if (loop_flag){
                vgmstream->loop_start_sample = loop_start/2/channel_count;
                vgmstream->loop_end_sample = loop_end/2/channel_count;
            }

            break;
        }

        case 0x2: { /* PCM 32 (Float) */
            goto fail; //probably unused/spec only
        }

        case 0x3: { /* PS ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = data_size*28/16/channel_count;
            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start*28/16/channel_count;
                vgmstream->loop_end_sample = loop_end*28/16/channel_count;
            }

            break;
        }

#ifdef VGM_USE_FFMPEG
        case 0x4:   /* ATRAC3 low (66 kbps, frame size 96, Joint Stereo) */
        case 0x5:   /* ATRAC3 mid (105 kbps, frame size 152) */
        case 0x6: { /* ATRAC3 high (132 kbps, frame size 192) */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[100];
            int32_t bytes, block_size, encoder_delay, joint_stereo;

            block_size    = (codec_id==4 ? 0x60 : (codec_id==5 ? 0x98 : 0xC0)) * vgmstream->channels;
            joint_stereo  = (codec_id==4); /* interleaved joint stereo (ch must be even) */

            /* MSF skip samples: from tests with MSEnc and real files (ex. TTT2 eddy.msf v43, v01 demos) seems like 1162 is consistent.
             * Atelier Rorona bt_normal01 needs it to properly skip the beginning garbage but usually doesn't matter.
             * (note that encoder may add a fade-in with looping/resampling enabled but should be skipped) */
            encoder_delay = 1162;
            vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_size) - encoder_delay;
            if (vgmstream->sample_rate==0xFFFFFFFF) /* some MSFv1 (Digi World SP) */
                vgmstream->sample_rate = 44100;//voice tracks seems to use 44khz, not sure about other tracks

            bytes = ffmpeg_make_riff_atrac3(buf, 100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;


            /* manually set skip_samples if FFmpeg didn't do it */
            if (ffmpeg_data->skipSamples <= 0) {
                ffmpeg_set_skip_samples(ffmpeg_data, encoder_delay);
            }

            /* MSF loop/sample values are offsets so trickier to adjust the skip_samples but this seems correct */
            if (loop_flag) {
                vgmstream->loop_start_sample = atrac3_bytes_to_samples(loop_start, block_size) /* - encoder_delay*/;
                vgmstream->loop_end_sample   = atrac3_bytes_to_samples(loop_end, block_size) - encoder_delay;
            }

            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 0x7: { /* MPEG (LAME MP3 of any quality) */
            /* delegate to FFMpeg, it can parse MSF files */
            ffmpeg_codec_data *ffmpeg_data = init_ffmpeg_offset(streamFile, header_offset, streamFile->get_size(streamFile) );
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* vgmstream->num_samples = ffmpeg_data->totalSamples; */ /* duration may not be set/inaccurate */
            vgmstream->num_samples = (int64_t)data_size * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
            if (loop_flag) {
                //todo properly apply encoder delay, which seems to vary between 1152 (1f), 528, 576 or 528+576
                int frame_size = ffmpeg_data->frameSize;
                vgmstream->loop_start_sample = (int64_t)loop_start * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
                vgmstream->loop_start_sample -= vgmstream->loop_start_sample==frame_size ? frame_size
                        : vgmstream->loop_start_sample % frame_size;
                vgmstream->loop_end_sample = (int64_t)loop_end * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
                vgmstream->loop_end_sample -= vgmstream->loop_end_sample==frame_size ? frame_size
                        : vgmstream->loop_end_sample % frame_size;
            }

            break;
        }
#endif
#if defined(VGM_USE_MPEG) && !defined(VGM_USE_FFMPEG)
        case 0x7: { /* MPEG (LAME MP3 of any quality) */
            int frame_size = 576; /* todo incorrect looping calcs */

            mpeg_codec_data *mpeg_data = NULL;
            coding_t ct;

            mpeg_data = init_mpeg_codec_data(streamFile, start_offset, &ct, vgmstream->channels);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;

            vgmstream->coding_type = ct;
            vgmstream->layout_type = layout_mpeg;
            vgmstream->num_samples = mpeg_bytes_to_samples(data_size, mpeg_data);
            vgmstream->num_samples -= vgmstream->num_samples % frame_size;
            if (loop_flag) {
                vgmstream->loop_start_sample = mpeg_bytes_to_samples(loop_start, mpeg_data);
                vgmstream->loop_start_sample -= vgmstream->loop_start_sample % frame_size;
                vgmstream->loop_end_sample = mpeg_bytes_to_samples(loop_end, mpeg_data);
                vgmstream->loop_end_sample -= vgmstream->loop_end_sample % frame_size;
            }
            vgmstream->interleave_block_size = 0;

            break;
        }
#endif

        default:  /* 8+: not defined */
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
