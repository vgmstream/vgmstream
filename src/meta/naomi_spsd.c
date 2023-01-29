#include "meta.h"
#include "../coding/coding.h"

/* SPSD - Naomi (arcade) and early Dreamcast streams [Guilty Gear X (Naomi), Crazy Taxi (Naomi), Virtua Tennis 2 (Naomi)] */
VGMSTREAM* init_vgmstream_spsd(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels, codec, flags, index;


    /* checks */
    if (!is_id32be(0x00,sf, "SPSD"))
        goto fail;
    /* .str: actual extension, rare [Shenmue (DC)]
     * .spsd: header id (maybe real ext is .PSD, similar to "SMLT" > .MLT) */
    if (!check_extensions(sf, "str,spsd"))
        goto fail;

    if (read_32bitBE(0x04,sf) != 0x01010004 && /* standard version */
        read_32bitBE(0x04,sf) != 0x00010004)   /* uncommon version [Crazy Taxi (Naomi)] */
        goto fail;


    codec = read_8bit(0x08,sf);
    flags = read_8bit(0x09,sf);
    index = read_16bitLE(0x0a,sf);
    data_size = read_32bitLE(0x0c,sf);
    //if (data_size + start_offset != get_streamfile_size(streamFile))
    //    goto fail; /* some rips out there have incorrect padding */

    //todo with 0x80 seems 0x2c is a loop_start_sample but must be adjusted to +1 block? (uncommon flag though)
    loop_flag = (flags & 0x80);
    channels = ((flags & 0x01) || (flags & 0x02)) ? 2 : 1; /* 0x02 is rare but looks normal (Virtua Tennis 2) */
    start_offset = 0x40;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x2A,sf);

    vgmstream->meta_type = meta_SPSD;
    switch (codec) {
        case 0x00: /* [Virtua Tennis 2 (Naomi), Club Kart: European Session (Naomi)] */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size,channels,16);
            vgmstream->loop_start_sample = read_32bitLE(0x2c,sf) + pcm_bytes_to_samples(0x2000*channels,channels,16);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0x01: /* [Virtua Tennis 2 (Naomi)] */
            vgmstream->coding_type = coding_PCM8;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size,channels,8);
            vgmstream->loop_start_sample = read_32bitLE(0x2c,sf) + pcm_bytes_to_samples(0x2000*channels,channels,8);
            vgmstream->loop_end_sample = vgmstream->num_samples;

            break;

        case 0x03: /* standard */
            vgmstream->coding_type = coding_AICA_int;
            vgmstream->num_samples = yamaha_bytes_to_samples(data_size,channels);
            vgmstream->loop_start_sample = /*read_32bitLE(0x2c,streamFile) +*/ yamaha_bytes_to_samples(0x2000*channels,channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        default:
            goto fail;
    }

    /* interleave index, maybe */
    switch(index) {
        case 0x0000:
            if (channels != 1) goto fail;
            vgmstream->layout_type = layout_none;
            break;

        case 0x000d:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2000;
            if (vgmstream->interleave_block_size)
                vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;
            break;

        case 0x00ff:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = data_size / channels;
            break;

    default:
        goto fail;
    }

    /* todo seems to decode slightly incorrectly in after certain data (loop section start?)
     *  may depend on values in 0x20 or 0x2c [ex. Marvel vs Capcom 2 (Naomi)]
     *  at 0x30(4*ch) is some config per channel but doesn't seem to affect ADPCM (found with PCM too) */
    {
        int i;
        for (i = 0; i < channels; i++) {
            vgmstream->ch[i].adpcm_step_index = 0x7f;
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
