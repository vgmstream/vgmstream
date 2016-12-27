#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* MSF header */
VGMSTREAM * init_vgmstream_ps3_msf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset, header_offset = 0;
    int32_t data_size, loop_start, loop_end;
    int loop_flag = 0;
  	int channel_count;
    int codec_id;

#ifdef VGM_USE_FFMPEG
	ffmpeg_codec_data *ffmpeg_data = NULL;
#endif


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("msf",filename_extension(filename))) goto fail;


    /* "WMSF" variation with a mini header over the MSFC header, same extension */
    if (read_32bitBE(0x00,streamFile) == 0x574D5346) {
        header_offset = 0x10;
    }
    start_offset = header_offset+0x40;

    /* usually 0x4D534643 "MSFC" */
    if (read_8bit(header_offset+0x0,streamFile) != 0x4D) goto fail; /* M */
    if (read_8bit(header_offset+0x1,streamFile) != 0x53) goto fail; /* S */
    if (read_8bit(header_offset+0x2,streamFile) != 0x46) goto fail; /* F */



    data_size = read_32bitBE(header_offset+0x0C,streamFile); /* without header*/
    if (data_size==0xFFFFFFFF) {
        size_t fileLength = get_streamfile_size(streamFile);
        data_size = fileLength - start_offset;
    }

    /* block_align/loop_type? = read_32bitBE(header_offset+0x14,streamFile);*/ /* 00/40 when no loop, 11/50/51/71 */

    loop_start = read_32bitBE(header_offset+0x18,streamFile);
    loop_end = read_32bitBE(header_offset+0x1C,streamFile); /* loop duration */
	loop_flag = loop_start != 0xFFFFFFFF;
    if (loop_flag) {
        if (loop_end==0xFFFFFFFF) {/* not seen */
            loop_end = data_size;
        } else {
            loop_end = loop_start + loop_end; /* usually equals data_size but not always */
            if ( loop_end > data_size)/* not seen */
                loop_end = data_size;
        }
    }

    channel_count = read_32bitBE(header_offset+0x8,streamFile);
    codec_id = read_32bitBE(header_offset+0x4,streamFile);
    

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
	vgmstream->channels = channel_count;

    /* Sample rate hack for strange files that don't have a specified frequency */
	if (read_32bitBE(header_offset+0x10,streamFile)==0x00000000)
		vgmstream->sample_rate = 48000;
	else
		vgmstream->sample_rate = read_32bitBE(header_offset+0x10,streamFile);


    vgmstream->meta_type = meta_PS3_MSF;

    switch (codec_id) {
        case 0x0: /* PCM (Big Endian) */
            {
                vgmstream->coding_type = coding_PCM16BE;
                vgmstream->num_samples = data_size/2/channel_count;
                
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
                vgmstream->num_samples = data_size*28/16/channel_count;

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
                  vgmstream->interleave_block_size = 0x10;
                }
            }
            break;
#ifdef VGM_USE_FFMPEG
        case 0x4: /* ATRAC3 (frame size 96) */
        case 0x5: /* ATRAC3 (frame size 152) */
        case 0x6: /* ATRAC3 (frame size 192) */
            /* delegate to FFMpeg, it can parse MSF files */
            ffmpeg_data = init_ffmpeg_offset(streamFile, header_offset, streamFile->get_size(streamFile) );
            if ( !ffmpeg_data ) goto fail;

            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->meta_type = meta_FFmpeg;
            vgmstream->codec_data = ffmpeg_data;

            vgmstream->num_samples = ffmpeg_data->totalSamples;

            if (loop_flag && ffmpeg_data->blockAlign > 0) {
                vgmstream->loop_start_sample = (loop_start / ffmpeg_data->blockAlign) * ffmpeg_data->frameSize;
                vgmstream->loop_end_sample = (loop_end / ffmpeg_data->blockAlign) * ffmpeg_data->frameSize;
            }

            break;
#endif
#ifdef VGM_USE_FFMPEG
        case 0x7: /* MPEG */
            /* delegate to FFMpeg, it can parse MSF files */
            ffmpeg_data = init_ffmpeg_offset(streamFile, header_offset, streamFile->get_size(streamFile) );
            if ( !ffmpeg_data ) goto fail;

            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->meta_type = meta_FFmpeg;
            vgmstream->codec_data = ffmpeg_data;

            /* TODO check CBR better (bitrate % X != 0?) */
            if (ffmpeg_data->bitrate == 0)
                goto fail;

            /* vgmstream->num_samples = ffmpeg_data->totalSamples; */ /* duration may not be set/inaccurate */
            vgmstream->num_samples = (int64_t)data_size * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
            if (loop_flag) {
                int frame_size = ffmpeg_data->frameSize;
                vgmstream->loop_start_sample = (int64_t)loop_start * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
                vgmstream->loop_start_sample -= vgmstream->loop_start_sample==frame_size ? frame_size
                        : vgmstream->loop_start_sample % frame_size;
                vgmstream->loop_end_sample = (int64_t)loop_end * ffmpeg_data->sampleRate * 8 / ffmpeg_data->bitrate;
                vgmstream->loop_end_sample -= vgmstream->loop_end_sample==frame_size ? frame_size
                        : vgmstream->loop_end_sample % frame_size;
            }

            break;
#endif
#if defined(VGM_USE_MPEG) && !defined(VGM_USE_FFMPEG)
        case 0x7: /* MPEG */
            {
                int frame_size = 576; /* todo incorrect looping calcs, MP3 can have other sizes */

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
                vgmstream->num_samples = mpeg_bytes_to_samples(data_size, &mi);
                vgmstream->num_samples -= vgmstream->num_samples % frame_size;
                if (loop_flag) {
                    vgmstream->loop_start_sample = mpeg_bytes_to_samples(loop_start, &mi);
                    vgmstream->loop_start_sample -= vgmstream->loop_start_sample % frame_size;
                    vgmstream->loop_end_sample = mpeg_bytes_to_samples(loop_end, &mi);
                    vgmstream->loop_end_sample -= vgmstream->loop_end_sample % frame_size;
                }
                vgmstream->interleave_block_size = 0;
            }
            break;
#endif
        default:
            goto fail;
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
                vgmstream->ch[i].offset=start_offset+vgmstream->interleave_block_size*i;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
#ifdef VGM_USE_FFMPEG
    if (ffmpeg_data) {
        free_ffmpeg(ffmpeg_data);
        if (vgmstream) vgmstream->codec_data = NULL;
    }
#endif
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
