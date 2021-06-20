#include "meta.h"
#include "../coding/coding.h"


/* Cstr - from Namco NuSound v1 games [Mr. Driller (GC), Star Fox Assault (GC), Donkey Konga (GC)] */
VGMSTREAM * init_vgmstream_cstr(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t interleave, first_skip;
    int loop_flag, channels, sample_rate;
    int loop_start, loop_end, num_samples, num_nibbles;


    /* checks */
    if (!check_extensions(sf,"dsp"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x43737472) /* "Cstr" */
        goto fail;

    /* 0x04: version (0x0066 = 0.66 as seen in nus_config .txt) */
    interleave = read_u16be(0x06, sf);
    /* 0x08: config? (volume/pan/etc?) */
    first_skip = read_s32be(0x0c, sf); /* first interleaved block has normal size, but starts late */
    loop_start = read_s32be(0x10, sf);
    /* 0x14: num samples LE or null in mono files */
    /* 0x18: sample rate LE or null in mono files */
    /* 0x01a: always 0x40 */
    channels = read_u8(0x1b, sf); /* mono only seen in R:Racing Evolution radio sfx */
    /* 0x1c: always null */

    /* next is DSP header, with some oddities:
     * - loop flag isn't always set vs Cstr's flag (won't have DSP loop_ps/etc)
     * - loop start nibbles can be wrong even with loop flag set
     * - wonky loop_ps as a result (other fields agree between channels) */

    num_samples = read_s32be(0x20 + 0x00, sf);
    num_nibbles = read_s32be(0x20 + 0x04, sf);
    sample_rate = read_s32be(0x20 + 0x08, sf);
  //loop_flag   = read_s16be(0x20 + 0x0c, sf);
  //loop_start  = read_s32be(0x20 + 0x10, sf);
    loop_end    = read_s32be(0x20 + 0x14, sf);

    loop_flag = (loop_start >= 0);
    start_offset = 0x20 + 0x60 * channels + first_skip;

    /* nonlooped tracks may not set first skip for no reason, but can be tested with initial p/s */
    if (!loop_flag && channels == 2 && first_skip == 0) {
        while (first_skip < 0x800) {
            if (read_u16be(0x20 + 0x3e, sf) == read_u8(start_offset + first_skip, sf) &&
                read_u16be(0x20 + 0x60 + 0x3e, sf) == read_u8(start_offset + first_skip + interleave, sf))
                break;
            first_skip += 0x08;
        }
        /* not found */
        if (first_skip == 0x800)
            first_skip = 0;
        else
            start_offset += first_skip;
    }
    if (first_skip > 0 && loop_start >= (interleave - first_skip))
        loop_start  = loop_start - (interleave - first_skip);

    loop_start = loop_start * 2;

    /* Mr. Driller oddity, unreliable loop flag */
    if (loop_end == num_nibbles) {
        loop_flag = 0;
    }

    /* Mr. Driller oddity, half nibbles */
    if (loop_end * 2 + 1 <= num_nibbles) {
        loop_end = loop_end * 2;
    }


    /* no loop_ps checks given how buggy the format is */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channels);

    vgmstream->loop_end_sample = dsp_nibbles_to_samples(loop_end) + 1;
    /* Donkey Konga 3 oddity, loop/num nibbles not correct vs final samples */
    if (vgmstream->loop_end_sample > num_samples)
        vgmstream->loop_end_sample = num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->interleave_first_block_size = interleave - first_skip;
    vgmstream->interleave_first_skip = first_skip;
    vgmstream->meta_type = meta_DSP_CSTR;

    dsp_read_coefs_be(vgmstream, sf, 0x3c, 0x60);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
