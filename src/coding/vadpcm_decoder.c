#include "coding.h"
#include "../util.h"


/* Decodes Silicon Graphics' N64 VADPCM, big brother of GC ADPCM.
 * Has external coefs like DSP, but allows tables up to 8 groups of 8 coefs (code book) and also
 * up to 8 history samples (order). In practice order must be 2, while files use 2~4 tables, so it
 * ends up being an overcomplex XA. Code respects this quirky configurable hist for doc purposes though.
 *
 * This code is based on N64SoundListTool decoding (by Ice Mario), with bits of the official SDK vadpcm tool
 * decompilation. I can't get proper sound from the later though, so not too sure about accuracy of this
 * implementation. Output sounds correct though.
 */

void decode_vadpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int order) {
    uint8_t frame[0x09] = {0};
    off_t frame_offset;
    int frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int i, j, k, o;
    int scale, index;

    int codes[16]; /* AKA ix */
    int16_t hist[8] = {0};
    int16_t out[16];
    int16_t* coefs;


    VGM_ASSERT_ONCE(order != 2, "VADPCM: wrong order=%i\n", order);
    if (order != 2) /* only 2 allowed "in the current implementation" */
        order = 2;

    /* up to 8 (hist[0]=oldest) but only uses latest 2 (order=2), so we don't save the whole thing ATM */
    hist[6] = stream->adpcm_history2_16;
    hist[7] = stream->adpcm_history1_16;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x09;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 16 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    scale = (frame[0] >> 4) & 0xF;
    index = (frame[0] >> 0) & 0xF;

    scale = 1 << scale;

    VGM_ASSERT_ONCE(index > 8, "DSP: incorrect index at %x\n", (uint32_t)frame_offset);
    if (index > 8) /* assumed */
        index = 8;
    coefs = &stream->vadpcm_coefs[index * (order*8) + 0];


    /* read and pre-scale all nibbles, since groups of 8 are needed */
    for (i = 0, j = 0; i < 16; i += 2, j++) {
        int n0 = (frame[j+1] >> 4) & 0xF;
        int n1 = (frame[j+1] >> 0) & 0xF;

        /* sign extend */
        if (n0 & 8)
            n0 = n0 - 16;
        if (n1 & 8)
            n1 = n1 - 16;

        codes[i+0] = n0 * scale;
        codes[i+1] = n1 * scale;
    }

    /* decode 2 sub-frames of 8 samples (maybe like this since N64 MIPS has registers to spare?) */
    for (j = 0; j < 2; j++) {
        /* SDK dec code and N64ST copy 8 codes to a tmp buf and has some complex logic to move out buf
         * around, but since that's useless N64 asm would be much more optimized... hopefully */
        int* sf_codes = &codes[j*8];
        int16_t* sf_out = &out[j*8];

        /* works with 8 samples at a time, related in twisted ways */
        for( i = 0; i < 8; i++) {
            int sample, delta = 0;

            /* in practice: delta = coefs[0][i] * hist[6] + coefs[1][i] * hist[7],
             * much like XA's coef1*hist1 + coef2*hist2 but with multi coefs */
            for (o = 0; o < order; o++) {
                delta += coefs[o*8 + i] * hist[(8 - order) + o];
            }

            /* adds all previous samples */
            for (k = i-1; k > -1; k--) {
                for (o = 1; o < order; o++) { /* assumed, since only goes coefs[1][k] */
                    delta += sf_codes[(i-1) - k] * coefs[(o*8) + k];
                }
            }

            /* scale-filter thing (also seen in DSP) */
            sample = (sf_codes[i] << 11);
            sample = (sample + delta) >> 11;
            if (sample > 32767)
                sample = 32767;
            else if (sample < -32768)
                sample = -32768;

            sf_out[i] = sample;
        }

        /* save subframe hist */
        for (i = 8 - order; i < 8; i++) {
            hist[i] = sf_out[i];
        }
    }


    /* copy samples last, since the whole thing is kinda complex to worry about half copying and stuff */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        outbuf[sample_count] = out[i];
        sample_count += channelspacing;
    }

    /* update hist once all frame is actually copied */
    if (first_sample + sample_count / channelspacing == samples_per_frame) {
        stream->adpcm_history2_16 = hist[6];
        stream->adpcm_history1_16 = hist[7];
    }
}

/*
int32_t vadpcm_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x09 * 16;
}
*/

/* Reads code book, linearly unlike original SDK, that does some strange reordering and pre-scaling
 * to reduce some loops. Format is 8 coefs per 'order' per 'entries' (max 8, but order is always 2). So:
 * - i: table index (selectable filter tables on every decoded frame)
 * - j: order index (coefs for prev N hist samples)
 * - k: coef index (multiplication coefficient for 8 samples in a sub-frame)
 * coefs[i * (order*8) + j * 8 + k * order] = coefs[i][j][k] */
void vadpcm_read_coefs_be(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, int order, int entries, int ch) {
    int i;

    if (entries > 8)
        entries = 8;
    VGM_ASSERT(order != 2, "VADPCM: wrong order %i found\n", order);
    if (order != 2)
        order = 2;

    /* assumes all channels use same coefs, never seen non-mono files */
    for (i = 0; i < entries * order * 8; i++) {
        vgmstream->ch[ch].vadpcm_coefs[i] = read_s16be(offset + i*2, sf);
    }
    vgmstream->codec_config = order;
}
