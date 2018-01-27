#include "meta.h"
#include "../coding/coding.h"

/* .XWC - Starbreeze games [Chronicles of Riddick: Assault on Dark Athena, Syndicate] */
VGMSTREAM * init_vgmstream_xwc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
	off_t start_offset;
	size_t data_size;
    int loop_flag, channel_count, codec;

    /* check extensions (.xwc is the extension of the bigfile, individual files don't have one) */
    if ( !check_extensions(streamFile,"xwc"))
        goto fail;

    if(read_32bitBE(0x00,streamFile) != 0x00040000 && /* version? */
       read_32bitBE(0x04,streamFile) != 0x00900000)
        goto fail;

    data_size = read_32bitLE(0x08, streamFile); /* including subheader */
	channel_count = read_32bitLE(0x0c, streamFile);
	/* 0x10: num_samples */
	/* 0x14: 0x8000? */
	codec = read_32bitBE(0x24, streamFile);
	/* 0x28: num_samples */
	/* 0x2c: config data? (first nibble: 0x4=mono, 0x8=stereo) */
	/* 0x30+: codec dependant */
    loop_flag = 0; /* seemingly not in the file */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = read_32bitLE(0x28, streamFile);
    vgmstream->meta_type = meta_XWC;

    switch(codec) {
#ifdef VGM_USE_MPEG
        case 0x4D504547: { /* "MPEG" (PS3) */
            mpeg_custom_config cfg = {0};

            start_offset = 0x800;
            vgmstream->num_samples = read_32bitLE(0x30, streamFile); /* with encoder delay */ //todo improve
            cfg.data_size = read_32bitLE(0x34, streamFile); //data_size - 0x28;

            vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = ((mpeg_codec_data*)vgmstream->codec_data)->sample_rate_per_frame;
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 0x584D4100: { /* "XMA\0" (X360) */
            uint8_t buf[0x100];
            int32_t bytes, seek_size, block_size, block_count, sample_rate;

            seek_size = read_32bitLE(0x30, streamFile);
            start_offset = 0x34 + seek_size + read_32bitLE(0x34+seek_size, streamFile) + 0x08;
            start_offset += (start_offset % 0x800) ? 0x800 - (start_offset % 0x800) : 0; /* padded */

            sample_rate = read_32bitBE(0x34+seek_size+0x10, streamFile);
            block_size = read_32bitBE(0x34+seek_size+0x1c, streamFile);
            block_count = read_32bitBE(0x34+seek_size+0x28, streamFile);
            /* others: scrambled RIFF fmt BE values */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size - start_offset - 0x28);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = sample_rate;
            break;
        }

        case 0x564F5242: { /* "VORB" (PC) */
            start_offset = 0x30;

            vgmstream->codec_data = init_ffmpeg_offset(streamFile, start_offset, data_size - start_offset - 0x28);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = read_32bitLE(start_offset + 0x28, streamFile);
            break;
        }
#endif
        default:
            goto fail;
    }

    if (vgmstream->sample_rate != 48000) { /* get from config data instead of codecs? */
        VGM_LOG("XWC: unexpected sample rate %i\n",vgmstream->sample_rate);
        goto fail;
    }

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
