#include "meta.h"
#include "../coding/coding.h"


/* .smp - Terminal Reality's Infernal Engine 'samples' [Ghostbusters: The Video Game (PS2/PS3/X360/PC/PSP), Chandragupta (PS2/PSP)] */
VGMSTREAM * init_vgmstream_smp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, codec, version, num_samples, bps;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile, "smp"))
        goto fail;

    version = read_32bitLE(0x00,streamFile);
    if (version != 0x05 &&  /* Ghostbusters (PS2), Mushroom Men (Wii) */
        version != 0x06 &&  /* Ghostbusters (PS3/X360/PC) */
        version != 0x07 &&  /* Ghostbusters (PSP) */
        version != 0x08)    /* Chandragupta (PS2/PSP), Street Cricket Champions 1/2 (PSP) */
        goto fail;

    /* 0x04~14: guid? */
    if (read_32bitLE(0x14,streamFile) != 0) /* reserved? */
        goto fail;
    num_samples   = read_32bitLE(0x18,streamFile);
    start_offset  = read_32bitLE(0x1c,streamFile);
    data_size     = read_32bitLE(0x20,streamFile);
    codec         = read_32bitLE(0x24,streamFile);
    channel_count = read_32bitLE(0x28,streamFile);
    bps           = read_32bitLE(0x2c,streamFile);
    sample_rate   = read_32bitLE(0x30,streamFile);

    loop_flag = 0;
    if (start_offset + data_size != get_streamfile_size(streamFile))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SMP;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch(codec) {
#ifdef VGM_USE_FFMPEG
        case 0x01: {
            int block_align, encoder_delay;
            if (bps != 16) goto fail;

            block_align = 0x98 * vgmstream->channels;
            encoder_delay = 0; /* 1024 looks ok, but num_samples needs to be adjusted too */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamFile, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case 0x02:
            if (bps != 4) goto fail;
            if (channel_count > 1) goto fail; /* not known */
            /* 0x34: standard DSP header, but LE */

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            dsp_read_coefs_le(vgmstream,streamFile,0x50,0x00);
            break;

        case 0x04:
            if (bps != 4) goto fail;
            if (!msadpcm_check_coefs(streamFile, 0x36))
                goto fail;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = 0x86*channel_count;
            break;

        case 0x06:
            if (bps != 4) goto fail;
            if (channel_count > 1) goto fail; /* not known */

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_none;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x07: {
            uint8_t buf[0x100];
            int bytes, block_size, block_count;

            if (bps != 16) goto fail;
            /* 0x34(0x28): XMA config/table? */

            block_size = 0x8000; /* assumed, @0x3e(2)? */
            block_count = data_size / block_size + (data_size % block_size ? 1 : 0); /* @0x54(2)? */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            //xma_fix_raw_samples(vgmstream, streamFile, start_offset,data_size, 0, ); //todo
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
