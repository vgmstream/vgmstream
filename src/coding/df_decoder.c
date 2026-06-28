#include "../base/codec_info.h"
#include "../vgmstream.h"
#include "../coding/coding.h"
#include "../base/sbuf.h"
#include "../base/decode_state.h"

/*
 * DreamFactory ADPCM/DPCM Codecs
 *
 * Titles: Dust 1996 3.1/95 - Titanic: Adventure Out of Time
 * v4.0 Codec:
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
 * v4.1 Codec:
 * A simpler 8-bit to 16-bit DPCM format. The input byte determines whether
 * it represents a delta value to be added to the current sample or a new
 * absolute sample value, based on its highest bit.
 *
 * DreamFactory 5 (D5) reuses these codecs plus standard IMA inside a blocked
 * SOUN stream, but resets codec state every block and renders v4.0/v4.1 at <<9
 * (the v4.x titles use <<8). The v5 v4.0/v4.1 variants below carry the block-reset
 * model;
 */

/* DC offset fix. v4.x callers pass shift=8 (canonical u8->s16); v5 uses 9. */
#define DF_V40_SAMPLE_TO_16BIT(sample, shift) ((int16_t)(((int8_t)(sample) - 0x40) << (shift)))

/* DreamFactory v4.0 ADPCM Decoder (8-bit to 8-bit) */

static bool decode_cf_df_v40(VGMSTREAM* v, sbuf_t* sdst) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    decode_state_t* ds = v->decode_state;
    int samples_to_do = ds->samples_left;
    sample_t* outbuf = sbuf_get_filled_buf(sdst);
    int i = 0;
    int8_t prev_sample = (int8_t)stream->adpcm_history1_16;

    /* Handle the very first sample if starting from the beginning */
    if (stream->offset == stream->channel_start_offset) {
        prev_sample = read_u8(stream->offset, stream->streamfile);
        stream->offset++;
        if (samples_to_do > 0) {
            outbuf[0] = DF_V40_SAMPLE_TO_16BIT(prev_sample, 8);
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
            outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev_sample, 8);
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
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(step_sample, 8);

                if (i >= samples_to_do) {
                    prev_sample = step_sample;
                    break;
                }

                /* Apply Low Nibble Delta */
                int8_t index_sample = step_sample + index_delta;
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(index_sample, 8);
                prev_sample = index_sample;
            }
        }
        /* Mode III: Repeat (RLE) */
        else {
            int count = (control_byte & 0x3f) + 1;
            for (int j = 0; j < count && i < samples_to_do; j++) {
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev_sample, 8);
            }
        }
    }

    stream->adpcm_history1_16 = prev_sample;
    return true;
}

/* DreamFactory v4.1 DPCM Decoder (16-bit) */

static bool decode_cf_df_v41(VGMSTREAM* v, sbuf_t* sdst) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    decode_state_t* ds = v->decode_state;
    int samples_to_do = ds->samples_left;
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
    return true;
}

/* DreamFactory 5 v4.0 ADPCM: same control stream as v4.x but the running sample is
 * seeded per block from adpcm_history1_16 (set by the block layout, NOT emitted) and
 * output is scaled <<9. The block layout points stream->offset at the first control
 * byte (block + 1).
 *
 * A Mode II/III run can be longer than one decode_buf call, so mid-run state is carried
 * across calls (otherwise a split run would lose its tail and read past the block):
 *   adpcm_step_index = run mode (0 idle, 2 Mode II, 3 Mode III)
 *   adpcm_history2_32 = samples/pairs left in the run
 *   adpcm_history4_32 = a pending Mode-II second sample is buffered
 *   adpcm_history3_32 = that pending sample
 * The layout clears all of these at each block boundary. */

/* Walk a v5 v4.0 control stream to count its output samples (no decode), used by the meta
 * (total num_samples) and the block layout (per-block current_block_samples). data = block
 * start, block_size = block input size; the running sample seed is byte[0], control bytes follow. */
int32_t cf_df_v5_get_samples(STREAMFILE* sf, off_t block_data, int block_size) {
    int32_t samples = 0;
    off_t p = block_data + 1;            /* skip the seed byte */
    off_t end = block_data + block_size;
    while (p < end) {
        uint8_t ctrl = read_u8(p++, sf);
        if ((ctrl & 0x80) == 0) {            /* Mode I: absolute */
            samples += 1;
        }
        else if ((ctrl & 0x40) == 0) {       /* Mode II: nibble pairs, consumes pair_count bytes */
            int pair_count = (ctrl & 0x3f) + 1;
            samples += 2 * pair_count;
            p += pair_count;
        }
        else {                            /* Mode III: RLE */
            samples += (ctrl & 0x3f) + 1;
        }
    }
    return samples;
}

static bool decode_cf_df_v5_v40(VGMSTREAM* v, sbuf_t* sdst) {
    VGMSTREAMCHANNEL* stream = &v->ch[0];
    decode_state_t* ds = v->decode_state;
    int samples_to_do = ds->samples_left;
    sample_t* outbuf = sbuf_get_filled_buf(sdst);
    int i = 0;

    int8_t prev = (int8_t)stream->adpcm_history1_16;
    int run_mode = stream->adpcm_step_index;
    int run_count = stream->adpcm_history2_32;
    int pending_valid = stream->adpcm_history4_32;
    int16_t pending = (int16_t)stream->adpcm_history3_32;

    while (i < samples_to_do) {
        /* flush a buffered Mode-II second sample from a previous call */
        if (pending_valid) {
            outbuf[i++] = pending;
            pending_valid = 0;
            continue;
        }
        /* Mode III: Repeat (RLE) */
        if (run_mode == 3) {
            while (run_count > 0 && i < samples_to_do) {
                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev, 9);
                run_count--;
            }
            if (run_count == 0) run_mode = 0;
            continue;
        }
        /* Mode II: DPCM Nibble Sequences */
        if (run_mode == 2) {
            while (run_count > 0 && i < samples_to_do) {
                uint8_t table_val = read_u8(stream->offset++, stream->streamfile);
                int8_t step_delta = (int8_t)table_val >> 4;
                int8_t index_delta = (int8_t)(table_val << 4) >> 4;
                int8_t s1 = prev + step_delta;
                int8_t s2 = s1 + index_delta;
                prev = s2;
                run_count--;

                outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(s1, 9);
                if (i < samples_to_do) {
                    outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(s2, 9);
                } else {
                    pending = DF_V40_SAMPLE_TO_16BIT(s2, 9);
                    pending_valid = 1;
                    break;
                }
            }
            if (run_count == 0) run_mode = 0;
            continue;
        }
        /* idle: read a control byte */
        uint8_t control_byte = read_u8(stream->offset++, stream->streamfile);
        if ((control_byte & 0x80) == 0) {        /* Mode I: absolute */
            prev = (int8_t)control_byte;
            outbuf[i++] = DF_V40_SAMPLE_TO_16BIT(prev, 9);
        } else if ((control_byte & 0x40) == 0) { /* Mode II */
            run_mode = 2;
            run_count = (control_byte & 0x3f) + 1;
        } else {                                 /* Mode III */
            run_mode = 3;
            run_count = (control_byte & 0x3f) + 1;
        }
    }

    stream->adpcm_history1_16 = prev;
    stream->adpcm_step_index = run_mode;
    stream->adpcm_history2_32 = run_count;
    stream->adpcm_history4_32 = pending_valid;
    stream->adpcm_history3_32 = pending;
    return true;
}

const codec_info_t cf_df_v40_decoder = {
    .decode_buf = decode_cf_df_v40,
};

const codec_info_t cf_df_v41_decoder = {
    .decode_buf = decode_cf_df_v41,
};

const codec_info_t cf_df_v5_v40_decoder = {
    .decode_buf = decode_cf_df_v5_v40,
};
