#include <math.h>
#include "coding.h"
#include "../util.h"

/* Westwood Studios ADPCM */
/* Based on Valery V. Anisimovsky's WS-AUD.txt */

static char WSTable2bit[4] = { -2, -1, 0, 1 };
static char WSTable4bit[16] = { -9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8 };

/* We pass in the VGMSTREAM here, unlike in other codings, because the decoder has to know about the block structure. */
void decode_ws(VGMSTREAM* vgmstream, int channel, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    VGMSTREAMCHANNEL* stream = &(vgmstream->ch[channel]);
    STREAMFILE* sf = stream->streamfile;
    int16_t hist = stream->adpcm_history1_16;
    off_t offset = stream->offset;
    int samples_left_in_frame = stream->ws_samples_left_in_frame;
    off_t header_offset = stream->ws_frame_header_offset;

    //int i;
    int32_t sample_count = 0;

    if (vgmstream->ws_output_size == vgmstream->current_block_size) {
        /* uncompressed pcm8 to pcm16 */
        for (int i = first_sample; i < first_sample + samples_to_do; i++) {
            outbuf[sample_count] = (read_u8(offset,sf) - 0x80) * 0x100;
            sample_count += channelspacing;
            offset++;
        }
    }
    else {
        if (first_sample == 0) {
            hist = 0x80;
            samples_left_in_frame = 0;
        }

        /* decompress */
        for (int i = first_sample; i < first_sample + samples_to_do; /* manually incremented */) {

            if (samples_left_in_frame == 0) {
                header_offset = offset;
                offset++;
            }

            uint8_t header = read_u8(header_offset, sf);
            uint8_t count = header & 0x3f;
            uint8_t code = header >> 6;
            switch (code) {    /* code */
                case 0: /* 2-bit ADPCM */
                    if (samples_left_in_frame == 0)
                        samples_left_in_frame = (count + 1) * 4;

                    /* read this frame up to samples_to_do */
                    for ( ; samples_left_in_frame>0 && i < first_sample + samples_to_do; i++) {

                        int twobit = ((count + 1) * 4 - samples_left_in_frame) % 4;
                        uint8_t sample = read_u8(offset,sf);
                        sample = (sample >> (twobit * 2)) & 0x3;
                        hist += WSTable2bit[sample];

                        if (hist < 0) hist = 0;
                        else if (hist > 0xff) hist = 0xff;

                        outbuf[sample_count] = (hist - 0x80) * 0x100;
                        sample_count += channelspacing;
                        samples_left_in_frame--;

                        if (twobit == 3)
                            offset++;  /* done with that byte */
                    }
                    break;

                case 1: /* 4-bit ADPCM */
                    if (samples_left_in_frame == 0)
                        samples_left_in_frame = (count + 1) * 2;

                    /* read this frame up to samples_to_do */
                    for ( ; samples_left_in_frame>0 && i < first_sample + samples_to_do; i++) {

                        int nibble = ((count + 1) * 4 - samples_left_in_frame) % 2;
                        uint8_t sample = read_u8(offset, sf);
                        if (nibble == 0)
                            sample &= 0xf;
                        else
                            sample >>= 4;
                        hist += WSTable4bit[sample];

                        if (hist < 0) hist = 0;
                        else if (hist > 0xff) hist = 0xff;

                        outbuf[sample_count] = (hist - 0x80) * 0x100;
                        sample_count += channelspacing;
                        samples_left_in_frame--;

                        if (nibble == 1)
                            offset++;  /* done with that byte */
                    }
                    break;

                case 2: /* no compression */
                    if (count & 0x20) { /* new delta */
                        /* Note no checks against samples_to_do here, at the top of the for loop
                         * we can always do at least one sample */

                        /* low 5 bits are a signed delta */
                        if (count & 0x10) {
                            hist -= ((count & 0x0f) ^ 0x0f) + 1;
                        } else {
                            hist += count & 0x0f;
                        }

                        /* Valery doesn't specify this, but clamp just in case */
                        if (hist < 0) hist = 0;
                        else if (hist > 0xff) hist = 0xff;

                        i++;

                        outbuf[sample_count] = (hist - 0x80) * 0x100;
                        sample_count += channelspacing;
                        samples_left_in_frame = 0; /* just one */
                    }
                    else {
                        /* copy bytes verbatim */
                        if (samples_left_in_frame == 0)
                            samples_left_in_frame = (count + 1);

                        /* read this frame up to samples_to_do */
                        for ( ; samples_left_in_frame > 0 && i < first_sample + samples_to_do; i++) {
                            hist = read_u8(offset,sf);
                            offset++;

                            outbuf[sample_count] = (hist - 0x80) * 0x100;
                            sample_count += channelspacing;
                            samples_left_in_frame--;
                        }
                    }
                    break;

                case 3: /* RLE */
                    if (samples_left_in_frame == 0)
                        samples_left_in_frame = (count + 1);

                    /* read this frame up to samples_to_do */
                    for ( ; samples_left_in_frame > 0 && i < first_sample + samples_to_do; i++) {
                        outbuf[sample_count] = (hist - 0x80) * 0x100;
                        sample_count += channelspacing;
                        samples_left_in_frame--;
                    }
                default:
                    break;
            }
        }
    }

    stream->offset = offset;
    stream->adpcm_history1_16 = hist;
    stream->ws_samples_left_in_frame = samples_left_in_frame;
    stream->ws_frame_header_offset = header_offset;
}
