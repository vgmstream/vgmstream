#include "meta.h"
#include "../coding/coding.h"

/* .BAF - Bizarre Creations bank file [Blur (PS3), Project Gotham Racing 4 (X360), Geometry Wars (PC)] */
VGMSTREAM * init_vgmstream_baf(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset, name_offset;
    size_t stream_size;
    uint32_t channel_count, sample_rate, num_samples, version, codec, tracks;
    int loop_flag, total_subsongs, target_subsong = sf->stream_index;
    uint32_t (*read_u32)(off_t,STREAMFILE*);

    /* checks */
    if (!is_id32be(0x00, sf, "BANK"))
        goto fail;

    if (!check_extensions(sf, "baf"))
        goto fail;

    /* use BANK size to check endianness */
    if (guess_endianness32bit(0x04,sf)) {
        read_u32 = read_u32be;
    } else {
        read_u32 = read_u32le;
    }

    /* 0x04: bank size */
    version = read_u32(0x08,sf);
    if (version != 0x03 && version != 0x04 && version != 0x05)
        goto fail;
    total_subsongs = read_u32(0x0c,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* - in v3 */
    /* 0x10: 0? */
    /* 0x11: bank name */
    /* - in v4/5 */
    /* 0x10: 1? */
    /* 0x11: padding flag? */
    /* 0x12: bank name */

    /* find target WAVE chunk */
    {
        int i;
        off_t offset = read_u32(0x04, sf);

        for (i = 0; i < total_subsongs; i++) {
            if (i+1 == target_subsong)
                break;
            offset += read_u32(offset+0x04, sf); /* WAVE size, variable per codec */

            /* skip companion "CUE " (found in 007: Blood Stone, contains segment cues) */
            if (is_id32be(offset+0x00, sf, "CUE ")) {
                offset += read_u32(offset+0x04, sf); /* CUE size */
            }
        }
        header_offset = offset;
    }

    /* parse header */
    if (!is_id32be(header_offset+0x00, sf, "WAVE"))
        goto fail;
    codec        = read_u32(header_offset+0x08, sf);
    name_offset  = header_offset + 0x0c;
    start_offset = read_u32(header_offset+0x2c, sf);
    stream_size  = read_u32(header_offset+0x30, sf);
    tracks = 0;

    switch(codec) {
        case 0x03: /* PCM16LE */
            switch(version) {
                case 0x03: /* Geometry Wars (PC) */
                    sample_rate     = read_u32(header_offset+0x38, sf);
                    channel_count   = read_u32(header_offset+0x40, sf);
                    /* no actual flag, just loop +15sec songs */
                    loop_flag = (pcm_bytes_to_samples(stream_size, channel_count, 16) > 15*sample_rate);
                    break;

                case 0x04: /* Project Gotham Racing 4 (X360) */
                    sample_rate     = read_u32(header_offset+0x3c, sf);
                    channel_count   = read_u32(header_offset+0x44, sf);
                    loop_flag       = read_u8(header_offset+0x4b, sf);
                    break;

                default:
                    goto fail;
            }

            num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
            break;

        case 0x07: /* PSX ADPCM (0x21 frame size) */
            if (version == 0x04 && read_u32(header_offset + 0x3c, sf) != 0) {
                /* Blur (Prototype) (PS3) */
                sample_rate     = read_u32(header_offset+0x3c, sf);
                channel_count   = read_u32(header_offset+0x44, sf);
                loop_flag       =  read_u8(header_offset+0x4b, sf);

                /* mini-header at the start of the stream */
                num_samples     = read_u32le(start_offset+0x04, sf) / 0x02; /* PCM size? */
                start_offset   += read_u32le(start_offset+0x00, sf);
                break;
            }

            switch (version) {
                case 0x04: /* Blur (PS3) */
                case 0x05: /* James Bond 007: Blood Stone (X360) */
                    sample_rate     = read_u32(header_offset+0x40, sf);
                    num_samples     = read_u32(header_offset+0x44, sf);
                    loop_flag       =  read_u8(header_offset+0x48, sf);
                    tracks          =  read_u8(header_offset+0x49, sf);
                    channel_count   =  read_u8(header_offset+0x4b, sf);

                    if (tracks) {
                        channel_count = channel_count * tracks;
                    }
                    break;
                default:
                    goto fail;
            }
            break;

        case 0x08: /* XMA1 */
            switch(version) {
                case 0x04: /* Project Gotham Racing (X360) */
                    sample_rate     = read_u32(header_offset+0x3c, sf);
                    channel_count   = read_u32(header_offset+0x44, sf);
                    loop_flag       =  read_u8(header_offset+0x54, sf) != 0;
                    break;

                case 0x05: /* James Bond 007: Blood Stone (X360) */
                    sample_rate     = read_u32(header_offset+0x40, sf);
                    channel_count   = read_u32(header_offset+0x48, sf);
                    loop_flag       =  read_u8(header_offset+0x58, sf) != 0;
                    break;

                default:
                    goto fail;
            }
            break;

        default:
            goto fail;
    }
    /* others: pan/vol? fixed values? (0x19, 0x10) */

    /* after WAVEs there may be padding then DATAs chunks, but offsets point after DATA size */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BAF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x03:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = num_samples;
            break;

        case 0x07:
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x21;

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = num_samples;
            break;

    #ifdef VGM_USE_FFMPEG
        case 0x08: {
            uint8_t buf[0x100];
            int bytes;

            bytes = ffmpeg_make_riff_xma1(buf,0x100, 0, stream_size, vgmstream->channels, vgmstream->sample_rate, 0);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* need to manually find sample offsets, it was a thing with XMA1 */
            {
                ms_sample_data msd = {0};

                msd.xma_version  = 1;
                msd.channels     = channel_count;
                msd.data_offset  = start_offset;
                msd.data_size    = stream_size;
                msd.loop_flag    = loop_flag;
                msd.loop_start_b = read_u32(header_offset+0x4c, sf);
                msd.loop_end_b   = read_u32(header_offset+0x50, sf);
                msd.loop_start_subframe  = (read_u8(header_offset+0x55, sf) >> 0) & 0x0f;
                msd.loop_end_subframe    = (read_u8(header_offset+0x55, sf) >> 4) & 0x0f;
                xma_get_samples(&msd, sf);

                vgmstream->num_samples = msd.num_samples; /* also at 0x58, but unreliable? */
                vgmstream->loop_start_sample = msd.loop_start_sample;
                vgmstream->loop_end_sample = msd.loop_end_sample;
            }

            xma_fix_raw_samples_ch(vgmstream, sf, start_offset, stream_size, channel_count, 1,1);
            break;
        }
    #endif

        default:
            VGM_LOG("BAF: unknown codec %x\n", codec);
            goto fail;
    }

    read_string(vgmstream->stream_name,0x20+1, name_offset,sf);


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* awful PS3 splits of the above with bad offsets and all */
VGMSTREAM * init_vgmstream_baf_badrip(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t WAVE_size, stream_size;
    off_t start_offset;
    long sample_count;
    int sample_rate;

    const int frame_size = 33;
    const int frame_samples = (frame_size-1) * 2;
    int channels;
    int loop_flag = 0;

    /* checks */
    if ( !check_extensions(streamFile, "baf") )
        goto fail;
    if (read_32bitBE(0,streamFile) != 0x57415645) /* "WAVE" */
        goto fail;
    WAVE_size = read_32bitBE(4,streamFile);
    if (WAVE_size != 0x4c) /* && WAVE_size != 0x50*/
        goto fail;
    if (read_32bitBE(WAVE_size,streamFile) != 0x44415441) /* "DATA"*/
        goto fail;
    /* check that WAVE size is data size */
    stream_size = read_32bitBE(0x30,streamFile);
    if (read_32bitBE(WAVE_size+4,streamFile)-8 != stream_size) goto fail;

    sample_count = read_32bitBE(0x44,streamFile);
    sample_rate = read_32bitBE(0x40,streamFile);
    /* unsure how to detect channel count, so use a hack */
    channels = (long long)stream_size / frame_size * frame_samples / sample_count;
    start_offset = WAVE_size + 8;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;

    vgmstream->coding_type = coding_PSX_cfg;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = frame_size;
    vgmstream->meta_type = meta_BAF;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
