#include "meta.h"
#include "../coding/coding.h"


/* KNON - from Donkey Kong: Barrel Blast (Wii) */
VGMSTREAM* init_vgmstream_knon(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks*/
    if (!is_id32be(0x00,sf, "KNON"))
        goto fail;

    /* .str: PCM files
     * .asr: DSP files */
    if (!check_extensions(sf, "str,asr"))
        goto fail;

    if (!is_id32be(0x08,sf, "WII "))
        goto fail;

    loop_flag = (read_32bitBE(0x44,sf) != 0);
    channels = 2;
    start_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x40,sf);
    
    switch (read_32bitBE(0x20,sf)) {
        case 0x4B415354: /* "KAST" */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->num_samples = dsp_bytes_to_samples(read_32bitBE(0x3C,sf), channels);
            vgmstream->loop_start_sample = dsp_bytes_to_samples(read_32bitBE(0x44,sf), channels);
            vgmstream->loop_end_sample = dsp_bytes_to_samples(read_32bitBE(0x48,sf), channels);
            vgmstream->interleave_block_size = 0x10;
            dsp_read_coefs_be(vgmstream, sf, 0x8c, 0x60);
            break;

        case 0x4B505354: /* "KPST" */
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->num_samples = pcm16_bytes_to_samples(read_32bitBE(0x3C,sf), channels);
            vgmstream->loop_start_sample = pcm16_bytes_to_samples(read_32bitBE(0x44,sf), channels);
            vgmstream->loop_end_sample = pcm16_bytes_to_samples(read_32bitBE(0x48,sf), channels);
            vgmstream->interleave_block_size = 0x10;
            break;

        default:
            goto fail;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_KNON;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
