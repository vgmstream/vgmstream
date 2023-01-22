#include "meta.h"
#include "../coding/coding.h"


/* RSD - from Radical Entertainment games */
VGMSTREAM* init_vgmstream_rsd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, name_offset;
    size_t data_size;
    int loop_flag, channels, sample_rate, interleave;
    uint32_t codec;
    uint8_t version;


    /* checks */
    if ((read_u32be(0x00,sf) & 0xFFFFFF00) != get_id32be("RSD\00"))
        goto fail;
    if (!check_extensions(sf,"rsd,rsp"))
        goto fail;

    loop_flag = 0;

    codec = read_u32be(0x04,sf);
    channels = read_s32le(0x08, sf);
    /* 0x0c: always 16? */
    sample_rate = read_s32le(0x10, sf);

    version = read_8bit(0x03, sf);
    switch(version) {
        case '2': /* known codecs: VAG/XADP/PCMB [The Simpsons: Road Rage] */
        case '3': /* known codecs: VAG/PCM/PCMB/GADP? [Dark Summit] */
            interleave   = read_u32le(0x14,sf); /* VAG only, 0x04 otherwise */
            start_offset = read_u32le(0x18,sf);
            name_offset  = 0;
            break;

        case '4': /* known codecs: VAG/PCM/RADP/PCMB [The Simpsons: Hit & Run, Tetris Worlds, Hulk] */
            /* 0x14: padding */
            /* 0x18: padding */
            interleave   = 0;
            start_offset = 0x800;
            name_offset  = 0;

            /* PCMB/PCM/GADP normally start early but sometimes have padding [The Simpsons: Hit & Run (GC/Xbox)] */
            if ((codec == get_id32be("PCM ") || codec == get_id32be("PCMB") || codec == get_id32be("GADP"))
                    && read_u32le(0x80,sf) != 0x2D2D2D2D)
                start_offset = 0x80;
            break;

        case '6': /* known codecs: VAG/XADP/WADP/RADP/OOGV/AT3+/XMA [Hulk 2, Crash Tag Team Racing, Crash: Mind over Mutant, Scarface] */
            /* 0x14: padding */
            name_offset  = 0x18; /* dev file path */
            interleave   = 0;
            start_offset = 0x800;
            break;

        default:
            goto fail;
    }

    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RSD;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case 0x50434D20:   /* "PCM " [Dark Summit (Xbox), Hulk (PC)] */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            break;

        case 0x50434D42:   /* "PCMB" [The Simpsons: Road Rage (GC), Dark Summit (GC)] */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            break;

        case 0x56414720:   /* "VAG " [The Simpsons: Road Rage (PS2), Crash Tag Team Racing (PSP)] */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (interleave == 0) ? 0x10 : interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            break;

        case 0x58414450:   /* "XADP" [The Simpsons: Road Rage (Xbox)], Crash Tag Team Racing (Xbox)] */
            vgmstream->coding_type = (channels > 2) ? coding_XBOX_IMA_mch : coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
            break;

        case 0x47414450:   /* "GADP" [Hulk (GC)] */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08; /* assumed, known files are mono */
            dsp_read_coefs_le(vgmstream,sf,0x14,0x2e); /* LE! */
            dsp_read_hist_le (vgmstream,sf,0x38,0x2e);

            vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
            break;

        case 0x57414450:   /* "WADP" [Crash: Mind Over Mutant (Wii)] */
            vgmstream->coding_type = coding_NGC_DSP_subint;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = 0x02;
            dsp_read_coefs_be(vgmstream,sf,0x1a4,0x28);
            dsp_read_hist_be (vgmstream,sf,0x1c8,0x28);

            vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
            break;

        case 0x52414450:   /* "RADP" [The Simpsons: Hit & Run (GC), Scarface (Wii)] */
            vgmstream->coding_type = coding_RAD_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = 0x14*channels;

            vgmstream->num_samples = data_size / 0x14 / channels * 32; /* bytes-to-samples */
            break;

#ifdef VGM_USE_VORBIS
        case 0x4F4F4756: { /* "OOGV" [Scarface (PC)] */
            ogg_vorbis_meta_info_t ovmi = {0};

            ovmi.meta_type = meta_RSD;
            close_vgmstream(vgmstream);
            vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
            if (!vgmstream) goto fail;
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x574D4120: { /* "WMA " [Scarface (Xbox)] */

            /* mini header + WMA header at start_offset */
            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset+0x08,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data); /* an estimation, sometimes cuts files a bit early */
          //vgmstream->num_samples = read_32bitLE(start_offset + 0x00, sf) / channels / 2; /* may be PCM data size, but not exact */
            vgmstream->sample_rate = read_32bitLE(start_offset + 0x04, sf);
            break;
        }

        case 0x4154332B: { /* "AT3+" [Crash of the Titans (PSP)] */
            int fact_samples = 0;

            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf, start_offset, &fact_samples);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = fact_samples;
            break;
        }

        case 0x584D4120: { /* "XMA " [Crash of the Titans (X360)-v1, Crash: Mind over Mutant (X360)-v2] */
            uint32_t chunk_size = read_32bitBE(0x800, sf);
            uint32_t seek_size = read_32bitBE(0x804, sf);
            uint32_t stream_size = read_32bitBE(0x808, sf);
            uint32_t chunk_offset = 0x80c;
            int old_xma2_version = read_u8(chunk_offset + 0x00, sf);

            start_offset = chunk_offset + chunk_size + seek_size;

            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, stream_size, chunk_offset, chunk_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* read PCM samples rather than full samples (dev trickery?) */
            switch (old_xma2_version) {
                case 0x03:
                    vgmstream->sample_rate = read_32bitBE(chunk_offset + 0x0c, sf);
                    vgmstream->num_samples = read_32bitBE(chunk_offset + 0x18, sf);
                    break;
                case 0x04:
                    vgmstream->num_samples = read_32bitBE(chunk_offset + 0x08, sf);
                    vgmstream->sample_rate = read_32bitBE(chunk_offset + 0x0c, sf);
                    break;
                default:
                    goto fail;
            }

            /* for some reason (dev trickery?) .rsd don't set skips in the bitstream, though they should */
            //xma_fix_raw_samples(vgmstream, sf, start_offset,xma_size, 0, 0,0);
            ffmpeg_set_skip_samples(vgmstream->codec_data, 512+64);
            break;
        }
#endif

        default:
            goto fail;
    }

    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
