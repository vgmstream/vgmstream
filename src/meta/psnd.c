#include "meta.h"
#include "../coding/coding.h"


/* PSND - from Polarbit games [Crash Bandicoot Nitro Kart 3D/2 (iOS), Reckless Racing 2 (Android/iOS)] */
VGMSTREAM* init_vgmstream_psnd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size, type;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "PSND"))
        return NULL;
    /* .psn: actual extension in exes/bigfiles */
    if (!check_extensions(sf, "psn"))
        return NULL;

    data_size = read_u32le(0x04, sf); /* after this field */
    type = read_u32le(0x08,sf);
    sample_rate = read_u16le(0x0c,sf);

    switch (type) {
        case 0x0030006: /* CBNK */
            channels    = read_u8(0x0E,sf);
            if (read_u8(0x0f, sf) != 16) // bps
                goto fail;
            start_offset = 0x10;
            break;

        case 0x0000004: /* RR */
            channels = 1;
            start_offset = 0x0e;
            break;

        default:
            goto fail;
    }
       
    data_size = data_size + 0x08 - start_offset;
    loop_flag = 0; // generally 22050hz music fully loops


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    switch (type) {
        case 0x0030006:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            break;

        case 0x0000004:
            vgmstream->coding_type = coding_DVI_IMA_mono;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = ima_bytes_to_samples(data_size, channels);

            // Reckless Getaway 2 (Android), Xenowerk (Android)
            vgmstream->allow_dual_stereo = true;
            break;

        default:
            goto fail;
    }

    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PSND;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
