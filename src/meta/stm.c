#include "meta.h"
#include "../coding/coding.h"

/* STM - from Angel Studios/Rockstar San Diego games (Red Dead Revolver, Midnight Club 2, TransWorld Surf) */
VGMSTREAM * init_vgmstream_stm(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;
	off_t start_offset;
    int loop_flag = 0, channel_count;
    int big_endian, bps, interleave, data_size, loop_start = 0, loop_end = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

	/* check extension, case insensitive
	 * .stm is the real ext but common, rename to .lstm or .stma/amts/ps2stm (older) */
    if ( !check_extensions(streamFile,"stm,lstm,stma,amts,ps2stm"))
        goto fail;

	/* check header */
    if ((read_32bitBE(0x00,streamFile) != 0x53544d41) &&    /* "SMTA" (LE) */
        (read_32bitBE(0x00,streamFile) != 0x414D5453))      /* "AMTS" (BE) */
        goto fail;
    /* 0x04: revision (696F/696B/696A/6969) */

    big_endian = (read_32bitBE(0x00,streamFile) == 0x414D5453);
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    start_offset = 0x800;

    interleave = read_32bit(0x08,streamFile);
    bps = read_32bit(0x10,streamFile);
    channel_count = read_32bit(0x14,streamFile);
    data_size = read_32bit(0x18,streamFile);
    loop_end = read_32bit(0x1c,streamFile); /* absolute offset */
    if (data_size + start_offset != get_streamfile_size(streamFile)) goto fail;

    if (big_endian) {
        /* GC AMTS have a regular DSP header beyond 0x20, just use what we need, no point on validating all fields */
        loop_flag = read_16bit(0x2c,streamFile);
    }
    else {
        /* PS2/Xbox STMA have either loop info or padding beyond 0x20 */
        if (read_32bit(0x20,streamFile) == 1)  { /* may contain 0xCCCCCCCC garbage */
            loop_flag = 1;
            loop_start = read_32bit(0x24,streamFile);
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

    vgmstream->sample_rate = read_32bit(0xc,streamFile);
    vgmstream->meta_type = meta_STM;
    vgmstream->layout_type = (channel_count > 1) ? layout_interleave : layout_none;

    switch(bps) {
        case 4:
            if (big_endian) { /* GC DSP ADPCM (TransWorld Surf GC) */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->interleave_block_size = interleave;

                vgmstream->num_samples = read_32bit(0x20,streamFile);
                vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bit(0x30,streamFile));
                vgmstream->loop_end_sample =  dsp_nibbles_to_samples(read_32bit(0x34,streamFile))+1;

                dsp_read_coefs_be(vgmstream, streamFile, 0x3c, 0x60);
                dsp_read_hist_be(vgmstream, streamFile, 0x60, 0x60);
            }
            else { /* DVI IMA ADPCM (Red Dead Revolver, Midnight Club 2) */
                vgmstream->coding_type = coding_DVI_IMA_int;
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


    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
