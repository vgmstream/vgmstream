#include "meta.h"
#include "../coding/coding.h"

/* BGW - from Final Fantasy XI (PC) music files */
VGMSTREAM * init_vgmstream_bgw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channel_count, loop_flag = 0;

    /* check extensions */
    if ( !check_extensions(streamFile, "bgw") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x42474d53 || /* "BGMS" */
        read_32bitBE(0x04,streamFile) != 0x74726561 || /* "trea" */
        read_32bitBE(0x08,streamFile) != 0x6d000000 )  /* "m\0\0\0" */
        goto fail;

    codec = read_32bitLE(0x0c,streamFile);
    file_size = read_32bitLE(0x10,streamFile);
    /*file_id = read_32bitLE(0x14,streamFile);*/
    block_size = read_32bitLE(0x18,streamFile);
    loop_start = read_32bitLE(0x1c,streamFile);
    sample_rate = (read_32bitLE(0x20,streamFile) + read_32bitLE(0x24,streamFile)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_32bitLE(0x28,streamFile);
    /*0x2c: unk (vol?) */
    /*0x2d: unk (0x10?) */
    channel_count = read_8bit(0x2e,streamFile);
    block_align = read_8bit(0x2f,streamFile);


    /* check file size with header value */
    if (file_size != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (loop_start > 0);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    vgmstream->meta_type = meta_FFXI_BGW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */

            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;

#ifdef VGM_USE_FFMPEG
        case 3: { /* ATRAC3 (encrypted) */
            uint8_t buf[0x100];
            int bytes, joint_stereo, skip_samples;
            ffmpeg_custom_config cfg;

            vgmstream->num_samples = block_size; /* atrac3_bytes_to_samples gives the same value */
            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            block_align  = 0xC0 * vgmstream->channels; /* 0x00 in header */
            joint_stereo = 0;
            skip_samples = 0;

            bytes = ffmpeg_make_riff_atrac3(buf, 0x100, vgmstream->num_samples, file_size - start_offset, vgmstream->channels, vgmstream->sample_rate, block_align, joint_stereo, skip_samples);
            if (bytes <= 0) goto fail;

            memset(&cfg, 0, sizeof(ffmpeg_custom_config));
            cfg.type = FFMPEG_BGW_ATRAC3;
            cfg.channels = vgmstream->channels;

            vgmstream->codec_data = init_ffmpeg_config(streamFile, buf,bytes, start_offset,file_size - start_offset, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* SPW (SEWave) - from  PlayOnline viewer for Final Fantasy XI (PC) */
VGMSTREAM * init_vgmstream_spw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channel_count, loop_flag = 0;

	/* check extensions */
	if ( !check_extensions(streamFile, "spw") )
        goto fail;

    /* check header */
    if (read_32bitBE(0,streamFile) != 0x53655761 || /* "SeWa" */
        read_32bitBE(4,streamFile) != 0x76650000)   /* "ve\0\0" */
        goto fail;

    /* check file size with header value */
    if (read_32bitLE(0x8,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    file_size = read_32bitLE(0x08,streamFile);
    codec = read_32bitLE(0x0c,streamFile);
    /*file_id = read_32bitLE(0x10,streamFile);*/
    block_size = read_32bitLE(0x14,streamFile);
    loop_start = read_32bitLE(0x18,streamFile);
    sample_rate = (read_32bitLE(0x1c,streamFile) + read_32bitLE(0x20,streamFile)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_32bitLE(0x24,streamFile);
    /*0x2c: unk (0x00?) */
    /*0x2d: unk (0x00/01?) */
    channel_count = read_8bit(0x2a,streamFile);
    block_align = read_8bit(0x2b,streamFile);
    /*0x2c: unk (0x01 when PCM, 0x10 when VAG?) */

    /* check file size with header value */
    if (file_size != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (loop_start > 0);

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    vgmstream->meta_type = meta_FFXI_SPW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */
            
            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;
            
        case 1: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            
            vgmstream->num_samples = block_size;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1);
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;
        default:
            goto fail;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
