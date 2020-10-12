#include "meta.h"
#include "../coding/coding.h"

/* LP/AP/LEP - from Enthusia: Professional Racing */
VGMSTREAM* init_vgmstream_ps2_enth(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave;
    int32_t data_size, loop_start; 
    uint32_t id;


    /* checks */
    /* .bin/lbin: assumed (no actual extensino in bigfiles)
     * .enth: fake */
    if (!check_extensions(sf, "bin,lbin,enth"))
        goto fail;

    id = read_32bitBE(0x00,sf);
    switch (id) {
        case 0x41502020: /* "AP  " */
        case 0x4C502020: /* "LP  " */
            sample_rate = read_u32le(0x08,sf);
            interleave  = read_u32le(0x0c,sf);
            loop_start  = read_u32le(0x14,sf);
            data_size   = read_u32le(0x18,sf);
            start_offset = read_32bitLE(0x1C,sf);
            break;

        case 0x4C455020: /* "LEP " */
            data_size   = read_u32le(0x08,sf);
            sample_rate = read_u16le(0x12,sf);
            loop_start  = read_u32le(0x58,sf);
            interleave  = 0x10;
            start_offset = 0x800;
            break;

        default:
            goto fail;
    }

    loop_flag = loop_start != 0;
    channels = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_ENTH;
    vgmstream->sample_rate = sample_rate;

    switch (id) {
        case 0x4C502020: /* "LP  " */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channels, 16);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            /* PCM data look different or encrypted
             * some PCM16 must be xored(?) with 0x8000, not sure when */
            goto fail;

        case 0x41502020: /* "AP  " */
        case 0x4C455020: /* "LEP " */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

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
