#include "../base/codec_info.h"
#include "../vgmstream.h"
#include "../coding/coding.h"
#include "../base/sbuf.h"

/*
 * DreamFactory ADPCM/DPCM Codecs
 *
 * Titles: Dust 1996 3.1/95 - Titanic: Adventure Out of Time
 * v4.0 Codec (decode_df_v40):
 * A custom 8-bit to 8-bit ADPCM format. It uses bitwise sign-extension
 * (high/low nibbles) to determine the next sample value based on the current one.
 * The compressed stream contains control codes to switch between three decoding modes:
 *
 * - Mode I: Sets the next sample to a new absolute value.
 * - Mode II: Decodes a series of samples using nibble-based deltas.
 * - Mode III: Repeats the previous sample value for a number of times.
 *
 * Has a DC offset of 64, instead of 128. Original is u8 type.
 *
 * Titles: Disney's Math/Reading Quest with Aladdin
 * v4.1 Codec (decode_df_v41):
 * A simpler 8-bit to 16-bit DPCM format. The input byte determines whether
 * it represents a delta value to be added to the current sample or a new
 * absolute sample value, based on its highest bit.
 *
 */

/* DC offset fix. */
#define DF_V40_SAMPLE_TO_16BIT(sample) ((int16_t)(((int8_t)(sample) - 0x40) << 8))

/* DreamFactory v4.0 ADPCM Decoder (8-bit to 8-bit) */

static bool decode_cf_df_v40(VGMSTREAM* v, sbuf_t* sdst) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    int samples_to_do = sdst->samples - sdst->filled;
    sample_t* outbuf = sbuf_get_filled_buf(sdst);
    int i = 0;
    int8_t prev_sample = (int8_t)stream->adpcm_history1_16;

    /* Handle the very first sample if starting from the beginning */
    if (stream->offset == stream->channel_start_offset) {
        prev_sample = read_u8(stream->offset, stream->streamfile);
        stream->offset++;
        if (samples_to_do > 0) {
            outbuf[0] = DF_V40_SAMPLE_TO_16BIT(prev_sample);
            samples_to_do--;
            outbuf++;
        }
    }

    while (i < samples_to_do) {
        uint8_t control_byte = read_u8(stream->offset, stream->streamfile);
        stream->offset++;

        /* Mode I: Absolute Value */
        if ((control_byte & 0x80) == 0) {
            prev_sample = (int8_t)control_byte;
            outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev_sample);
        }
        /* Mode II: DPCM Nibble Sequences */
        else if ((control_byte & 0x40) == 0) {
            int count = (control_byte & 0x3f) + 1;
            for (int j = 0; j < count && i < samples_to_do; j++) {
                uint8_t table_val = read_u8(stream->offset, stream->streamfile);
                stream->offset++;

                /* High Nibble Sign-Extension */
                int8_t step_delta = (int8_t)table_val >> 4;

                /* Low Nibble Sign-Extension */
                int8_t index_delta = (int8_t)(table_val << 4) >> 4;

                /* Apply High Nibble Delta */
                int8_t step_sample = prev_sample + step_delta;
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(step_sample);

                if (i >= samples_to_do) {
                    prev_sample = step_sample;
                    break;
                }

                /* Apply Low Nibble Delta */
                int8_t index_sample = step_sample + index_delta;
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(index_sample);
                prev_sample = index_sample;
            }
        }
        /* Mode III: Repeat (RLE) */
        else {
            int count = (control_byte & 0x3f) + 1;
            for (int j = 0; j < count && i < samples_to_do; j++) {
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev_sample);
            }
        }
    }

    stream->adpcm_history1_16 = prev_sample;
    sdst->filled += (sdst->samples - sdst->filled) - samples_to_do + i;
    return true;
}

/* DreamFactory v4.1 DPCM Decoder (16-bit) */

static bool decode_cf_df_v41(VGMSTREAM* v, sbuf_t* sdst) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    int samples_to_do = sdst->samples - sdst->filled;
    sample_t* outbuf = sbuf_get_filled_buf(sdst);
    int16_t current_sample = stream->adpcm_history1_16; // Technically should be DPCM but still "ADPCM" framework.

    for (int i = 0; i < samples_to_do; i++) {
        uint8_t input_byte = read_u8(stream->offset, stream->streamfile);
        stream->offset++;

        if ((input_byte & 0x80) == 0) {
            /* Delta Mode: 7-bit delta shifted left */
            int16_t delta = (int16_t)(input_byte << 9);
            delta >>= 4; /* Arithmetic shift restores sign */
            current_sample += delta;
        } else {
            /* Absolute Mode: New high bits */
            current_sample = (int16_t)(input_byte << 9);
        }
        outbuf[i] = current_sample;
    }

    stream->adpcm_history1_16 = current_sample;
    sdst->filled += samples_to_do;
    return true;
}

const codec_info_t cf_df_v40_decoder = {
    .decode_buf = decode_cf_df_v40,
};

const codec_info_t cf_df_v41_decoder = {
    .decode_buf = decode_cf_df_v41,
};