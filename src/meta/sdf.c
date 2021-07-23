#include "meta.h"
#include "../coding/coding.h"

typedef enum { PSX, PCM, DSP } sdf_codec;


/* SDF - from Beyond Reality games */
VGMSTREAM* init_vgmstream_sdf(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels, sample_rate, interleave, coefs_offset;
    sdf_codec codec;


    /* checks */
    if (!check_extensions(sf,"sdf"))
        goto fail;
    if (!is_id32be(0x00,sf, "SDF\0"))
        goto fail;
    if (read_u32le(0x04,sf) != 3) /* version? */
        goto fail;

    data_size = read_u32le(0x08,sf);
    start_offset = get_streamfile_size(sf) - data_size;

    switch(start_offset) {
        case 0x18:
            if (read_u32le(0x10,sf) > 6) {
                /* Hugo Magic in the Troll Woods (NDS) */
                /* 0x0c: some size? */
                sample_rate     = read_s32le(0x10,sf);
                channels        = read_u8(0x14,sf);
                /* 0x14: 1? */
                interleave      = read_u16le(0x16,sf);
                codec = PCM;
            }
            else {
                /* Agent Hugo: Lemoon Twist (PS2) */
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
            data_size       = read_u32le(0x20,sf); /* usable size */
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
        case PCM:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

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
