#include "meta.h"
#include "../coding/coding.h"

/* .XWC - Starbreeze games [Chronicles of Riddick: Assault on Dark Athena, Syndicate] */
VGMSTREAM* init_vgmstream_xwc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, extra_offset;
    size_t data_size;
    int loop_flag, channels, codec, num_samples;


    /* checks */
    /* .xwc: extension of the bigfile, individual files don't have one */
    if (!check_extensions(sf,"xwc"))
        goto fail;


    /* version */
    if (read_32bitBE(0x00,sf) == 0x00030000 &&
        read_32bitBE(0x04,sf) == 0x00900000) { /* The Darkness */
        data_size = read_32bitLE(0x08, sf) + 0x1c; /* not including subheader */
        channels = read_32bitLE(0x0c, sf);
        /* 0x10: num_samples */
        /* 0x14: 0x8000? */
        /* 0x18: null */
        codec = read_32bitBE(0x1c, sf);
        num_samples = read_32bitLE(0x20, sf);
        /* 0x24: config data >> 2? (0x00(1): channels; 0x01(2): ?, 0x03(2): sample_rate) */
        extra_offset = 0x28;
    }
    else if (read_32bitBE(0x00,sf) == 0x00040000 &&
             read_32bitBE(0x04,sf) == 0x00900000) { /* Riddick, Syndicate */
        data_size = read_32bitLE(0x08, sf) + 0x24; /* not including subheader */
        channels = read_32bitLE(0x0c, sf);
        /* 0x10: num_samples */
        /* 0x14: 0x8000? */
        codec = read_32bitBE(0x24, sf);
        num_samples = read_32bitLE(0x28, sf);
        /* 0x2c: config data >> 2? (0x00(1): channels; 0x01(2): ?, 0x03(2): sample_rate) */
        /* 0x30+: codec dependant */
        extra_offset = 0x30;
    }
    else {
        goto fail;
    }

    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XWC;
    vgmstream->num_samples = num_samples;

    switch(codec) {
#ifdef VGM_USE_MPEG
        case 0x4D504547: { /* "MPEG" (PS3) */
            mpeg_custom_config cfg = {0};

            start_offset = 0x800;
            vgmstream->num_samples = read_32bitLE(extra_offset+0x00, sf); /* with encoder delay */ //todo improve
            cfg.data_size = read_32bitLE(extra_offset+0x04, sf); /* without padding */

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = mpeg_get_sample_rate(vgmstream->codec_data);
            break;
        }
#endif
#ifdef VGM_USE_FFMPEG
        case 0x584D4100: { /* "XMA\0" (X360) */
            uint32_t seek_size, chunk_size, chunk_offset;
            int block_size, block_count, sample_rate;

            seek_size  = read_32bitLE(extra_offset + 0x00, sf);
            chunk_size = read_32bitLE(extra_offset + 0x04 + seek_size, sf);
            chunk_offset = extra_offset + 0x04 + seek_size + 0x04;

            data_size = read_32bitLE(chunk_offset + chunk_size + 0x00, sf);
            start_offset = chunk_offset + chunk_size + 0x04;
            start_offset += (start_offset % 0x800) ? 0x800 - (start_offset % 0x800) : 0; /* padded */

            if (chunk_size == 0x34) { /* new XMA2 */
                sample_rate = read_32bitLE(extra_offset+0x04+seek_size+0x08, sf);
                block_size  = read_32bitLE(extra_offset+0x04+seek_size+0x20, sf);
                block_count = data_size / block_size;
                /* others: standard RIFF XMA2 fmt? */
            }
            else if (chunk_size == 0x2c) { /* old XMA2 (not fully valid?) */
                sample_rate = read_32bitBE(extra_offset+0x04+seek_size+0x10, sf);
                block_size  = read_32bitBE(extra_offset+0x04+seek_size+0x1c, sf);
                block_count = read_32bitBE(extra_offset+0x04+seek_size+0x28, sf);
                /* others: scrambled RIFF fmt BE values */
            }
            else {
                goto fail;
            }

            vgmstream->sample_rate = sample_rate;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, start_offset, data_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, block_count);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, 0,0); /* samples are ok, fix delay */
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case 0x564F5242: { /* "VORB" (PC) */
            start_offset = 0x30;
            data_size = data_size - start_offset;

            vgmstream->sample_rate = read_32bitLE(start_offset + 0x28, sf);

            vgmstream->codec_data = init_ogg_vorbis(sf, start_offset, data_size, NULL);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

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
