#include "meta.h"
#include "../coding/coding.h"


/* .smp - Terminal Reality's Infernal Engine 'samples' [Ghostbusters: The Video Game (PS2/PS3/X360/PC/PSP), Chandragupta (PS2/PSP)] */
VGMSTREAM* init_vgmstream_smp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, extra_offset;
    int loop_flag, channels, sample_rate, codec, version, num_samples, bps;
    size_t data_size;


    /* checks */
    if (!check_extensions(sf, "smp"))
        goto fail;

    version = read_u32le(0x00,sf);
    if (version != 0x05 &&  /* Ghostbusters (PS2), Mushroom Men (Wii) */
        version != 0x06 &&  /* Ghostbusters (PS3/X360/PC) */
        version != 0x07 &&  /* Ghostbusters (PSP) */
        version != 0x08)    /* Chandragupta (PS2/PSP), Street Cricket Champions 1/2 (PSP), Guilty Party (Wii) */
        goto fail;

    /* 0x04~14: guid? */
    if (read_u32le(0x14,sf) != 0) /* reserved? */
        goto fail;
    num_samples   = read_s32le(0x18,sf);
    start_offset  = read_u32le(0x1c,sf);
    data_size     = read_u32le(0x20,sf);
    codec         = read_u32le(0x24,sf);
    /* smaller header found in Guilty Party (Wii) */
    if (version == 0x08 && start_offset == 0x80) {
        channels      = read_u8(0x28,sf);
        bps           = read_u8(0x29,sf);
        sample_rate   = read_u16le(0x2a,sf);
        extra_offset  = 0x2c; /* coefs only */
    }
    else {
        channels      = read_u32le(0x28,sf);
        bps           = read_u32le(0x2c,sf);
        sample_rate   = read_u32le(0x30,sf);
        extra_offset  = 0x34 + 0x1c; /* standard DSP header, but LE */
    }

    loop_flag = 0;
    if (start_offset + data_size != get_streamfile_size(sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
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

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case 0x02:
            if (bps != 4) goto fail;
            if (channels > 1) goto fail; /* not known */

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            dsp_read_coefs_le(vgmstream, sf, extra_offset, 0x00);
            //todo adpcm hist
            break;

        case 0x04:
            if (bps != 4) goto fail;
            if (!msadpcm_check_coefs(sf, 0x36))
                goto fail;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = 0x86*channels;
            break;

        case 0x06:
            if (bps != 4) goto fail;
            if (channels > 1) goto fail; /* not known */

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
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            //xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, ); //todo
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
