#include "meta.h"
#include "../coding/coding.h"

/* MSF - Sony's PS3 SDK format (MultiStream File) */
VGMSTREAM* init_vgmstream_msf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint32_t data_size, loop_start = 0, loop_end = 0;
    uint32_t codec, flags;
    int loop_flag, channels, sample_rate;


    /* checks */
    /* .msf: standard
     * .msa: Sonic & Sega All-Stars Racing (PS3)
     * .at3: Silent Hill HD Collection (PS3), Z/X Zekkai no Crusade (PS3)
     * .mp3: Darkstalkers Resurrection (PS3)
     * .str: Pac-Man and the Ghostly Adventures (PS3) */
    if (!check_extensions(sf,"msf,msa,at3,mp3,str"))
        goto fail;

    /* check header "MSF" + version-char, usually:
     *  0x01, 0x02, 0x30="0", 0x35="5", 0x43="C" (last/most common version) */
    if ((read_u32be(0x00,sf) & 0xffffff00) != 0x4D534600) /* "MSF\0" */
        goto fail;

    start_offset = 0x40;

    codec = read_u32be(0x04,sf);
    channels = read_s32be(0x08,sf);
    data_size = read_u32be(0x0C,sf); /* without header */
    if (data_size == 0xFFFFFFFF) /* unneeded? */
        data_size = get_streamfile_size(sf) - start_offset;
    sample_rate = read_s32be(0x10,sf);

    /* byte flags, not in MSFv1 or v2
     *  0x01/02/04/08: loop marker 0/1/2/3
     *  0x10: resample options (force 44/48khz)
     *  0x20: VBR MP3 source (encoded with min/max quality options, may end up being CBR)
     *  0x40: joint stereo MP3 (apparently interleaved stereo for other formats)
     *  0x80+: (none/reserved) */
    flags = read_u32be(0x14,sf);
    /* sometimes loop_start/end is set with flag 0x10, but from tests it only loops if 0x01/02 is set
     * 0x10 often goes with 0x01 but not always (Castlevania HoD); Malicious PS3 uses flag 0x2 instead */
    loop_flag = (flags != 0xffffffff) && ((flags & 0x01) || (flags & 0x02));

    /* loop markers (marker N @ 0x18 + N*(4+4), but in practice only marker 0 is used) */
    if (loop_flag) {
        loop_start = read_u32be(0x18,sf);
        loop_end = read_u32be(0x1C,sf); /* loop duration */
        loop_end = loop_start + loop_end; /* usually equals data_size but not always */
        if (loop_end > data_size) /* not seen */
            loop_end = data_size;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSF;
    vgmstream->sample_rate = sample_rate;
    if (vgmstream->sample_rate == 0) /* some MSFv1 (PS-ADPCM only?) [Megazone 23 - Aoi Garland (PS3)] */
        vgmstream->sample_rate = 48000;

    switch (codec) {
        case 0x00:   /* PCM (Big Endian) [MSEnc tests] */
        case 0x01: { /* PCM (Little Endian) [Smash Cars (PS3)] */
            vgmstream->coding_type = codec==0 ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            if (loop_flag){
                vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channels, 16);
                vgmstream->loop_end_sample = pcm_bytes_to_samples(loop_end, channels, 16);
            }

            break;
        }

        case 0x02: { /* PCM 32 (Float) */
            goto fail; /* probably unused/spec only */
        }

        case 0x03: { /* PS ADPCM [Smash Cars (PS3)] */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            if (loop_flag) {
                vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start,channels);
                vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end,channels);
            }

            break;
        }

#ifdef VGM_USE_FFMPEG
        case 0x04:   /* ATRAC3 low (66 kbps, frame size 96, Joint Stereo) [Silent Hill HD (PS3)] */
        case 0x05:   /* ATRAC3 mid (105 kbps, frame size 152) [Atelier Rorona (PS3)] */
        case 0x06: { /* ATRAC3 high (132 kbps, frame size 192) [Tekken Tag Tournament HD (PS3)] */
            int block_align, encoder_delay;

            /* MSF skip samples: from tests with MSEnc and real files (ex. TTT2 eddy.msf v43, v01 demos) seems like 1162 is consistent.
             * Atelier Rorona bt_normal01 needs it to properly skip the beginning garbage but usually doesn't matter.
             * (note that encoder may add a fade-in with looping/resampling enabled but should be skipped) */
            encoder_delay = 1024 + 69*2;
            block_align   = (codec==4 ? 0x60 : (codec==5 ? 0x98 : 0xC0)) * vgmstream->channels;
            vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_align) - encoder_delay;
            if (vgmstream->sample_rate == -1) /* some MSFv1 (Digi World SP) */
                vgmstream->sample_rate = 44100; /* voice tracks seems to use 44khz, not sure about other tracks */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* MSF loop/sample values are offsets so trickier to adjust but this seems correct */
            if (loop_flag) {
                /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
                vgmstream->loop_start_sample = atrac3_bytes_to_samples(loop_start, block_align); //- encoder_delay
                vgmstream->loop_end_sample   = atrac3_bytes_to_samples(loop_end, block_align) - encoder_delay;
            }

            break;
        }
#endif
#if defined(VGM_USE_MPEG)
        case 0x07: { /* MPEG (LAME MP3) [Dengeki Bunko Fighting Climax (PS3), Asura's Wrath (PS3)-vbr] */
            int is_vbr = (flags & 0x20); /* must calc samples/loop offsets manually */

            vgmstream->codec_data = init_mpeg(sf, start_offset, &vgmstream->coding_type, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_get_samples_clean(sf, start_offset, data_size, &loop_start, &loop_end, is_vbr);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            /* MPEG here seems stripped from ID3/Xing headers, loops are frame offsets */

            /* encoder delay varies between 1152 (1f), 528, 576, etc; probably not actually skipped */
            break;
        }
#elif defined(VGM_USE_FFMPEG)
        case 0x07: { /* MPEG (LAME MP3) [Dengeki Bunko Fighting Climax (PS3), Asura's Wrath (PS3)-vbr] */
            int is_vbr = (flags & 0x20); /* must calc samples/loop offsets manually */

            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset, 0);;
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_get_samples_clean(sf, start_offset, data_size, &loop_start, &loop_end, is_vbr);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;
        }
#endif

        default:  /* 0x08+: not defined */
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
