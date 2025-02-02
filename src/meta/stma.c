#include "meta.h"
#include "../coding/coding.h"

/* STM - from Angel Studios/Rockstar San Diego games [Red Dead Revolver (PS2), Spy Hunter 2 (PS2/Xbox)] */
VGMSTREAM* init_vgmstream_stma(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    int big_endian, bps, interleave, data_size, loop_start = 0, loop_end = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "STMA") &&      /* LE */
        !is_id32be(0x00,sf, "AMTS"))        /* BE */
        goto fail;
    /* .stm: real extension
     * .lstm: for plugins */
    if (!check_extensions(sf,"stm,lstm"))
        goto fail;

    /* 0x04: revision (696F/696B/696A/6969) */

    big_endian = is_id32be(0x00,sf, "AMTS");
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    start_offset = 0x800;

    interleave = read_32bit(0x08,sf);
    bps = read_32bit(0x10,sf);
    channel_count = read_32bit(0x14,sf);
    data_size = read_32bit(0x18,sf);
    loop_end = read_32bit(0x1c,sf); /* absolute offset */
    if (data_size + start_offset != get_streamfile_size(sf)) goto fail;

    if (big_endian) {
        /* GC AMTS have a regular DSP header beyond 0x20, just use what we need, no point on validating all fields */
        loop_flag = read_16bit(0x2c,sf);
    }
    else {
        /* PS2/Xbox STMA have either loop info or padding beyond 0x20 */
        if (read_32bit(0x20,sf) == 1)  { /* may contain 0xCCCCCCCC garbage */
            loop_flag = 1;
            loop_start = read_32bit(0x24,sf);
            /* 0x28 (32b * ch): loop start hist+step  per channel */ //todo setup
        }
#if 0
        /* this works for some files that do full repeats, but also repeats many SFX */
        else if (data_size != loop_end && data_size != loop_end - 0x100) { /* data_size vs adjusted loop_end */
            loop_flag = 1;
        }
#endif
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bit(0xc,sf);
    vgmstream->meta_type = meta_STMA;
    vgmstream->layout_type = (channel_count > 1) ? layout_interleave : layout_none;

    switch(bps) {
        case 4:
            if (big_endian) { /* GC DSP ADPCM (TransWorld Surf GC) */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->interleave_block_size = interleave;

                vgmstream->num_samples = read_32bit(0x20,sf);
                vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bit(0x30,sf));
                vgmstream->loop_end_sample =  dsp_nibbles_to_samples(read_32bit(0x34,sf))+1;

                dsp_read_coefs_be(vgmstream, sf, 0x3c, 0x60);
                dsp_read_hist_be(vgmstream, sf, 0x60, 0x60);
            }
            else { /* DVI IMA ADPCM (Red Dead Revolver, Midnight Club 2) */
                vgmstream->coding_type = coding_DVI_IMA_mono;
                /* 'interleave not' reliable, strange values and rarely needs 0x80 */
                vgmstream->interleave_block_size = (interleave == 0xc000) ? 0x80 : 0x40;

                vgmstream->num_samples = ima_bytes_to_samples(data_size, vgmstream->channels);
                vgmstream->loop_start_sample = loop_start;
                vgmstream->loop_end_sample = ima_bytes_to_samples(loop_end - start_offset, vgmstream->channels);
            }

            break;

        case 16: /* PCM (Spy Hunter 2 PS2, rare) */
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->interleave_block_size = 0x02; /* interleave not always reliable */

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, vgmstream->channels, bps);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = pcm_bytes_to_samples(loop_end - start_offset, vgmstream->channels, bps);

            break;

        default:
            VGM_LOG("STM: unknown bps %i\n", bps);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
