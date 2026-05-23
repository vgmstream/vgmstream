#include "meta.h"
#include "../coding/coding.h"

typedef enum { PSX, PCM16, PCM8, IMA, DSP } sdf_codec_t;


/* SDF - from Beyond Reality games */
VGMSTREAM* init_vgmstream_sdf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels, sample_rate, interleave, coefs_offset;
    sdf_codec_t codec;


    /* checks */
    if (!is_id32be(0x00,sf, "SDF\0"))
        return NULL;
    if (!check_extensions(sf,"sdf"))
        return NULL;

    if (read_u32le(0x04,sf) != 3) /* version? */
        return NULL;
    data_size = read_u32le(0x08,sf);
    start_offset = get_streamfile_size(sf) - data_size;

    switch(start_offset) {
        case 0x18:
            if (read_u32le(0x10,sf) > 6) {
                /* Hugo Magic in the Troll Woods (NDS), ATV Fever (DSi), ATV Quad Kings (NDS) */
                // 0x0c: tick count?
                sample_rate     = read_s32le(0x10,sf);
                int subcodec    =    read_u8(0x14,sf);
                channels        =    read_u8(0x15,sf); // assumed
                interleave      = read_u16le(0x16,sf);

                switch(subcodec) {
                    case 0x00: codec = PCM8; break;  // ATV Fever BGM
                    case 0x01: codec = PCM16; break; // Hugo, ATV Quad Kings
                    case 0x02: codec = IMA;  break;  // ATV Fever SFX
                    default:
                        return NULL;
                }
                
            }
            else {
                /* Agent Hugo: Lemoon Twist (PS2), Crazy Golf World Tour (PS2) */
                sample_rate     = read_s32le(0x0c,sf);
                channels        = read_u32le(0x10,sf);
                interleave      = read_u32le(0x14,sf);
                codec = PSX;
            }

            break;

        case 0x78: /* Gummy Bears Mini Golf (3DS) */
            sample_rate     = read_s32le(0x10,sf);
            channels        = read_u32le(0x14,sf);
            interleave      = read_u32le(0x18,sf);
            coefs_offset    = 0x1c;
            codec = DSP;
            break;

        case 0x84: /* Mr. Bean's Wacky World (Wii) */
            sample_rate     = read_s32le(0x10,sf);
            channels        = read_u32le(0x14,sf);
            interleave      = read_u32le(0x18,sf);
            data_size       = read_u32le(0x20,sf); // usable size
            coefs_offset    = 0x28;
            codec = DSP;
            break;

        default:
            goto fail;
    }

    loop_flag = 1;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDF;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case PCM16:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            break;

        case PCM8:
            vgmstream->coding_type = coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm8_bytes_to_samples(data_size, channels);
            break;

        case IMA:
            if (channels != 1) goto fail;
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = channels == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = interleave;

            for (int i = 0; i < channels; i++) {
                vgmstream->ch[i].adpcm_history1_16 = read_s16le(0x18 + i*0x04, sf);
                vgmstream->ch[i].adpcm_step_index = read_u8(0x1a + i*0x04, sf);
                if (vgmstream->ch[i].adpcm_step_index < 0 || vgmstream->ch[i].adpcm_step_index > 88)
                    goto fail;
            }

            start_offset += 0x04;
            data_size -= 0x04;
            vgmstream->num_samples = ima_bytes_to_samples(data_size, channels);
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            if (interleave && channels)
                vgmstream->interleave_last_block_size = (data_size % (interleave * channels)) / channels;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            if (vgmstream->interleave_block_size == 0) /* Gummy Bears Mini Golf */
                vgmstream->interleave_block_size = data_size / channels;

            vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);

            dsp_read_coefs_le(vgmstream, sf, coefs_offset+0x00,0x2e);
            dsp_read_hist_le (vgmstream, sf, coefs_offset+0x24,0x2e);
            break;

        default:
            goto fail;
    }

    /* most songs simply repeat; don't loop if too short (in seconds) */
    if (vgmstream->num_samples > 10*sample_rate) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
