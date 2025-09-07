#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "ka1a_dec.h"
#include "ka1a_dec_data.h"
#include "../../util/reader_get.h"

/* Decodes Koei Tecmo's KA1A, a fairly simple transform-based (FFT) mono codec.
 *
 * The codec seems nameless (it has a "_CODECNAME" string) so this is named after streamed files'
 * fourCC. It's somewhat inefficient (not very packed) but simple so maybe designed for speed.
 * OG code isn't too optimized though.
 *
 * Reverse engineered from exes, thanks to Kelebek1 and AceKombat for help and debugging.
 * Output has been compared to memdumps and should be accurate with minor +-diffs (vs MSVC 22 /O2).
 * 
 * Even though some parts can be simplified/optimized code tries to emulate what source code
 * may look like, undoing unrolled/vectorized parts. Functions marked as 'inline' don't exist in
 * decomp but surely were part of the source code, while 'unused' args may be remants/compilation details.
 * 
 * If you are going to use this info/code elsewhere kindly credit your sources. It's the right thing to do.
 */


// Gets frame info based on bitrate mode, to unpack 1 frame.
// OG code calls this per frame but codec is CBR (single bitrate index) plus values 
// could be precalculated per bitrate index (remnant of VBR or more complex modes?)
static void get_frame_info(int bitrate_index, int* p_steps_size, int* p_coefs_size) {
    int coefs_bits = 0;
    int steps_bits = 0;

    // first 8 bands use 8-bit codes and step is implicit
    for (int i = 0; i < 8; i++) {
        int codes = BAND_CODES[bitrate_index][i];
        coefs_bits += 8 * codes;
    }

    if (bitrate_index <= 5) {
        // lower bitrate modes have one 8-bit code, rest is 4-bit
        coefs_bits += (MAX_BANDS - 8) * 8;
        for (int i = 8; i < MAX_BANDS; i++) {
            int step_bits = BAND_STEP_BITS[i];
            int codes = BAND_CODES[bitrate_index][i];
            steps_bits += step_bits * codes;
            coefs_bits += 4 * (codes - 1);
        }
    }
    else {
        // higher bitrate modes use 8-bit codes
        for (int i = 8; i < MAX_BANDS; i++) {
            int step_bits = BAND_STEP_BITS[i];
            int codes = BAND_CODES[bitrate_index][i];
            steps_bits += step_bits * codes;
            coefs_bits += 8 * codes;
        }
    }

    // bits to bytes + padding
    *p_steps_size = (steps_bits + 7) >> 3;
    *p_coefs_size = (coefs_bits + 7) >> 3;
}

// Helper used in related functions, but not during decode. Note that 'mode' must be validated externally (-5..5).
// In practice values are: 0x60, 0x68, 0x73, 0x7d, 0x8c, 0x9b, 0xad, 0xc2, 0xd7, 0xed, 0x102.
static int get_frame_size(int bitrate_mode) {
    int scalefactor_size = 0x04;
    int steps_size = 0;
    int coefs_size = 0;
    get_frame_info(bitrate_mode + BITRATE_INDEX_MODIFIER, &steps_size, &coefs_size);
    return scalefactor_size + steps_size + coefs_size;
}


// Convert 8-bit signed code as exp 
// (note that 0.086643398 being float is important to get results closer to memdumps)
static inline float unpack_convert_code(uint8_t code, float scalefactor) {
    float coef;
    if (code) {
        float code_f = (int8_t)code;
        if (code & 0x80) {
            code_f = -code_f;
            scalefactor = -scalefactor;
        }

        coef = expf((code_f - 127.0f) * 0.086643398f) * scalefactor;
    }
    else {
        coef = 0.0;
    }

    return coef;
}

// Adjust current coef by -1.0..1.0 (4-bit subcode values 0..14 * 1/7 to -1.0..1.0; code 15 seems unused).
// (note that 0.14285715f being float is important to get results closer to memdumps)
static inline float unpack_convert_subcode(uint8_t code, float coef) {
    return ((code * 0.14285715f) - 1.0f) * coef;
}

// Get N bits (max 8) from data, MSB order.
// Doesn't check boundaries, but should never past src as bits come from fixed tables.
static inline int unpack_get_bits(uint8_t* src, int* p_byte_pos, int* p_bit_pos, int bits) {
    int value = 0;
    int byte_pos = *p_byte_pos;
    int bit_pos = *p_bit_pos;

    int next_bitpos = bit_pos + bits;
    if (next_bitpos > 8) {
        // read between 2 bytes
        if (next_bitpos <= 16) { // more shouldn't happen
            uint32_t mask_lo = (1 << (8 - bit_pos)) - 1;
            uint32_t mask_hi = (1 << (next_bitpos - 8)) - 1;
            uint8_t code_lo = src[byte_pos+0];
            uint8_t code_hi = src[byte_pos+1];
            value = ((code_hi & mask_hi) << (8 - bit_pos)) + ((code_lo >> bit_pos) & mask_lo);
        }
    }
    else {
        // read in current byte
        uint32_t mask = (1 << bits) - 1;
        uint8_t code = src[byte_pos];
        value = (code >> bit_pos) & mask;
    }

    bit_pos += bits;
    if (next_bitpos >= 8) {
        bit_pos = next_bitpos - 8;
        byte_pos++;
    }

    *p_byte_pos = byte_pos;
    *p_bit_pos = bit_pos;
    return value;
}

// Unpack a single frame into quantized spectrum coefficients, packed like this:
// - 1 scalefactor (32-bit float)
// - N coef sub-positions aka steps (4-7 bits) per higher bands (8..21)
// - N codes (8-bit) per lower bands (0..7), of implicit positions
// - 1 main code (8-bit) per higher bands 8..21 then (N-1) coefs (8 or 4-bit) per bands
//
// Each code is converted to a coef then saved to certain position to dst buf.
// Lower bitrate modes use 4-bit codes that are relative to main coef (* +-1.0).
//
// Bands encode less coefs than dst may hold, so 'positions' are used to put coefs
// non-linearly, where unset indexes are 0 (dst must be memset before calling unpack frame).
// dst should be 1024, though usually only lower 512 are used (max step is 390 + ((1<<7) - 1)).
static void unpack_frame(uint8_t* src, float* dst, int steps_size, void* unused, int bitrate_index) {

    // copy coefs counts as they may be modified below
    int band_codes_tmp[MAX_BANDS];
    for (int i = 0; i < MAX_BANDS; i++) {
        band_codes_tmp[i] = BAND_CODES[bitrate_index][i];
    }

    // read base scalefactor (first 4 bytes) and setup buffers
    float scalefactor = get_f32le(src);
    uint8_t* src_steps = &src[0x04];
    uint8_t* src_codes = &src[0x04 + steps_size];

    // negative scalefactor signals more/less codes for some bands (total doesn't change though)
    if (scalefactor < 0.0f) {
        scalefactor = -scalefactor;

        int mod = BITRATE_SUBMODE[bitrate_index];
        for (int i = 8; i < 12; i++) {
            band_codes_tmp[i] += mod;
        }
        for (int i = 17; i < 21; i++) {
            band_codes_tmp[i] -= mod;
        }
    }

    // coefs from lower bands (in practice fixed to 5 * 8)
    int code_pos = 0;
    for (int band = 0; band < 8; band++) {
        int band_codes = band_codes_tmp[band];
        for (int i = 0; i < band_codes; i++) {
            uint8_t code = src_codes[code_pos];
            dst[code_pos] = unpack_convert_code(code, scalefactor);
            code_pos++;
        }
    }

    // simple bitreading helpers (struct?)
    int br_bytepos = 0;
    int br_bitpos = 0; // in current byte

    int subcode_pos = code_pos + (MAX_BANDS - 8); // position after bands 8..21 main coef

    uint8_t code;
    float coef;
    int substep;

    if (bitrate_index <= 5) {
        // lower bitrates encode 1 main 8-bit coef per band and rest is main * +-1.0, position info in a bitstream
        bool high_flag = false;
        for (int band = 8; band < MAX_BANDS; band++) {
            int band_codes = band_codes_tmp[band];
            int band_step = BAND_STEPS[band];
            int step_bits = BAND_STEP_BITS[band];

            substep = unpack_get_bits(src_steps, &br_bytepos, &br_bitpos, step_bits);

            code = src_codes[code_pos];
            code_pos++;

            coef = unpack_convert_code(code, scalefactor);
            dst[band_step + substep] = coef;

            for (int i = 1; i < band_codes; i++) {
                substep = unpack_get_bits(src_steps, &br_bytepos, &br_bitpos, step_bits);

                code = src_codes[subcode_pos];
                if (high_flag)
                    subcode_pos++;

                uint8_t subcode = high_flag ? 
                    (code >> 4) & 0x0F : 
                    (code >> 0) & 0x0F;

                high_flag = !high_flag;

                dst[band_step + substep] = unpack_convert_subcode(subcode, coef);
            }
        }
    }
    else {
        // higher bitrates encode all coefs normally, but still use lower bitrates' ordering scheme (see above)
        for (int band = 8; band < MAX_BANDS; band++) {
            int band_codes = band_codes_tmp[band];
            int band_step = BAND_STEPS[band];
            int step_bits = BAND_STEP_BITS[band];

            substep = unpack_get_bits(src_steps, &br_bytepos, &br_bitpos, step_bits);

            code = src_codes[code_pos];
            code_pos++;

            coef = unpack_convert_code(code, scalefactor);
            dst[band_step + substep] = coef;

            for (int i = 1; i < band_codes; i++) {
                substep = unpack_get_bits(src_steps, &br_bytepos, &br_bitpos, step_bits);

                code = src_codes[subcode_pos];
                subcode_pos++;

                coef = unpack_convert_code(code, scalefactor);
                dst[band_step + substep] = coef;
            }
        }
    }
}


static void transform_twiddles(int points, float* real, float* imag, const float* tw_real, const float* tw_imag) {
    for (int i = 0; i < points; i++) {
        float coef_real = real[i];
        float coef_imag = imag[i];
        float twid_real = tw_real[i];
        float twid_imag = tw_imag[i];

        real[i] = (twid_real * coef_real) - (twid_imag * coef_imag);
        imag[i] = (twid_imag * coef_real) + (twid_real * coef_imag);
    }
}

static inline void transform_bit_reversal_permutation(int points, float* real, float* imag) {
    const int half = points >> 1;

    int j = 0;
    for (int i = 1; i < points; i++) {

        // j is typically calculated via subs of m, unsure if manual or compiler optimization
        j = half ^ j;
        int m = half;
        while (m > j) {
            m >>= 1;
            j = m ^ j;
        }

        if (i < j) {
            float coef_real = real[i];
            float coef_imag = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = coef_real;
            imag[j] = coef_imag;
        }
    }
}

static void transform_fft(int points, void* unused, float* real, float* imag, const float* cos_table, const float* sin_table) {
    const int half = points >> 1;

    transform_bit_reversal_permutation(points, real, imag);

    // these are actually the same value, so OG compilation only uses the cos_table one; added both for completeness
    float w_real_base = cos_table[points >> 3];
    float w_imag_base = sin_table[points >> 3];

    // FFT computation using twiddle factors and sub-ffts, probably some known optimization
    for (int m = 4; m <= points; m <<= 1) { // 0.. (log2(256) / 2)
        int m4 = m >> 2;

        for (int j = m4; j > 0; j >>= 2) {
            int min = m4 - j;
            int max = m4 - (j >> 1);
            int i_md = min + 2 * m4;

            for (int k = min; k < max; k++) {
                int i_lo = i_md - m4;
                int i_hi = i_md + m4;

                float coef_im_a = imag[k] - imag[i_lo];
                float coef_re_a = real[k] - real[i_lo];
                real[k] = real[i_lo] + real[k];
                imag[k] = imag[i_lo] + imag[k];

                float coef_re_b = real[i_hi] - real[i_md];
                float coef_im_b = imag[i_hi] - imag[i_md];
                float tmp_ra_ib = coef_re_a - coef_im_b;
                float tmp_rb_ia = coef_re_b + coef_im_a;
                float tmp_ib_ra = coef_im_b + coef_re_a;
                float tmp_ia_rb = coef_im_a - coef_re_b;

                real[i_md] = real[i_hi] + real[i_md];
                imag[i_md] = imag[i_hi] + imag[i_md];
                real[i_lo] = tmp_ra_ib;
                imag[i_lo] = tmp_rb_ia;
                real[i_hi] = tmp_ib_ra;
                imag[i_hi] = tmp_ia_rb;

                i_md++;
            }
        }

        if (m >= points)
            continue;

        for (int j = m4; j > 0; j >>= 2) {
            int min = m + m4 - j;
            int max = m + m4 - (j >> 1);
            int i_md = min + 2 * m4;

            for (int k = min; k < max; k++) {
                int i_lo = i_md - m4;
                int i_hi = i_md + m4;

                float coef_im_a = imag[k] - imag[i_lo];
                float coef_re_a = real[k] - real[i_lo];
                real[k] = real[i_lo] + real[k];
                imag[k] = imag[i_lo] + imag[k];

                float coef_re_b = real[i_hi] - real[i_md];
                float coef_im_b = imag[i_hi] - imag[i_md];
                float tmp_ra_ib = coef_re_a - coef_im_b;
                float tmp_rb_ia = coef_re_b + coef_im_a;
                float tmp_ib_ra = coef_im_b + coef_re_a;
                float tmp_ia_rb = coef_im_a - coef_re_b;

                real[i_md] = real[i_hi] + real[i_md];
                imag[i_md] = imag[i_hi] + imag[i_md];
                real[i_lo] = (tmp_rb_ia + tmp_ra_ib) * w_real_base;
                imag[i_lo] = (tmp_rb_ia - tmp_ra_ib) * w_real_base;
                real[i_hi] = (tmp_ia_rb - tmp_ib_ra) * w_imag_base;
                imag[i_hi] = (-tmp_ia_rb - tmp_ib_ra) * w_imag_base;

                i_md++;
            }
        }

        int tmp_j = half;
        for (int m2 = m * 2; m2 < points; m2 += m) {
            // ???
            int tmp_m = half;
            for (tmp_j ^= tmp_m; tmp_m > tmp_j; tmp_j ^= tmp_m) {
                tmp_m = tmp_m >> 1;
            }

            int table_index = tmp_j >> 2;
            float w_real1 = cos_table[table_index];
            float w_imag1 = -sin_table[table_index];
            float w_real3 = cos_table[table_index * 3]; 
            float w_imag3 = -sin_table[table_index * 3];

            for (int j = m4; j > 0; j >>= 2) {
                int min = m2 + m4 - j;
                int max = m2 + m4 - (j >> 1);
                int i_md = min + 2 * m4;

                for (int k = min; k < max; k++) {
                    int i_lo = i_md - m4;
                    int i_hi = i_md + m4;

                    float coef_im_a = imag[k] - imag[i_lo];
                    float coef_re_a = real[k] - real[i_lo];
                    real[k] = real[i_lo] + real[k];
                    imag[k] = imag[i_lo] + imag[k];

                    float coef_im_b = imag[i_hi] - imag[i_md];
                    float coef_re_b = real[i_hi] - real[i_md];
                    float tmp_ra_ib = coef_re_a - coef_im_b;
                    float tmp_rb_ia = coef_re_b + coef_im_a;
                    float tmp_ib_ra = coef_im_b + coef_re_a;
                    float tmp_ia_rb = coef_im_a - coef_re_b;

                    real[i_md] = real[i_hi] + real[i_md];
                    imag[i_md] = imag[i_hi] + imag[i_md];
                    real[i_lo] = (tmp_ra_ib * w_real1) - (tmp_rb_ia * w_imag1);
                    imag[i_lo] = (tmp_ra_ib * w_imag1) + (tmp_rb_ia * w_real1);
                    real[i_hi] = (tmp_ib_ra * w_real3) - (tmp_ia_rb * w_imag3);
                    imag[i_hi] = (tmp_ib_ra * w_imag3) + (tmp_ia_rb * w_real3);

                    i_md++;
                }
            }
        }
    }

    // final swapping
    for (int m = half; m > 0; m >>= 2) {
        int min = half - m;
        int max = half - (m >> 1);

        for (int k = min; k < max; k++) {
            float coef_im = imag[k] - imag[k + half];
            float coef_re = real[k] - real[k + half];
            real[k] = real[k + half] + real[k];
            imag[k] = imag[k + half] + imag[k];
            real[k + half] = coef_re;
            imag[k + half] = coef_im;
        }
    }
}

// Transform unpacked time-domain coefficients (spectrum) to samples using inverse FFT.
// Seemingly a variation/simplification of the Cooley-Tukey algorithm (radix-4?).
static void transform_frame(void* unused1, float* src, float* dst, void* unused2, float* fft_buf) {
    float* real = fft_buf;
    float* imag = fft_buf + 256;

    // initialize buffers from src
    for (int i = 0; i < 256; i++) {
        real[i]       = src[i * 2];
        imag[255 - i] = src[i * 2 + 1];
    }

    transform_twiddles(256, real, imag, TWIDDLES_REAL, TWIDDLES_IMAG);
    transform_fft(256, NULL, real, imag, COS_TABLE, SIN_TABLE);
    transform_twiddles(256, real, imag, TWIDDLES_REAL, TWIDDLES_IMAG);

    // Scale results by (1 / 512)
    for (int i = 0; i < 256; i++) {
        real[i] *= 0.001953125f;
        imag[i] *= 0.001953125f;
    }

    // Reorder output (input buf may be reused as output here as there is no overlap).
    // Note that input is 512 coefs but output is 1024 samples (externally combined with prev samples)
    int pos = 0;
    for (int i = 0; i < 128; i++) {
        dst[pos++] = real[128 + i];
        dst[pos++] = -imag[127 - i];
    }
    for (int i = 0; i < 256; i++) {
        dst[pos++] = imag[i];
        dst[pos++] = -real[255 - i];
    }
    for (int i = 0; i < 128; i++) {
        dst[pos++] = -real[i];
        dst[pos++] = imag[255 - i];
    }
}

// Decodes a block of frames (see .h)
//
// To get 512 samples decoder needs to combine samples from prev + current frame (MP3 granule-style?).
// though will only output samples from current. prev-frame can be optionally used to setup overlapping
// samples with 'setup_flag'. Since decoding current-frame will also setup the overlap for next frame,
// prev data and predecode-flag are only needed on init or after seeking.
// 
// Original decoder expects 2 blocks in src (1 frame * channels * tracks): src[0] = prev, src[block-size] = curr
// (even if prev isn't used). This isn't very flexible, so this decoder expects only 1 block.
// Probably setup this odd way due to how data is read/handled in KT's engine.
static void decode_frame(unsigned char* src, int tracks, int channels, float* dst, int bitrate_mode, int setup_flag, float* prev, float* temp) {
    float* fft_buf = &temp[0]; //size 512 * 2
    float* coefs = &temp[512 * 2]; //size 512 * 2

    int bitrate_index = bitrate_mode + BITRATE_INDEX_MODIFIER;
    int steps_size = 0;
    int coefs_size = 0;
    get_frame_info(bitrate_index, &steps_size, &coefs_size);
    int frame_size = 0x04 + steps_size + coefs_size;

    // decode 'prev block of frames' (optional as it just setups 'prev' buf, no samples are written)
    if (setup_flag) {
        uint8_t* src_block = &src[0]; // 1st block in src

        for (int track = 0; track < tracks; track++) {
            int frame_num = channels * track;

            for (int ch = 0; ch < channels; ch++) {
                uint8_t* frame = &src_block[frame_num * frame_size];

                memset(coefs, 0, FRAME_SAMPLES * sizeof(float));
                unpack_frame(frame, coefs, steps_size, NULL, bitrate_index);
                transform_frame(NULL, coefs, coefs, NULL, fft_buf);

                int interleave = frame_num * FRAME_SAMPLES;
                for (int i = 0; i < FRAME_SAMPLES; i++) {
                    // save samples for 'current block of frames' and overlap
                    prev[interleave + i] = coefs[512 + i] * OVERLAP_WINDOW[511 - i];
                }

                frame_num++;
            }
        }
    }

    if (setup_flag) // OG MOD: changed to expect only 1 block per call
        return;

    // decode 'current block of frames' (writes 512 samples, plus setups 'prev' buf)
    {
        //uint8_t* src_block = &src[channels * tracks * frame_size]; // 2nd block in src in OG code
        uint8_t* src_block = &src[0]; // OG MOD: changed to expect only 1 block  per call

        for (int track = 0; track < tracks; track++) {
            int frame_num = channels * track;

            float* dst_track = &dst[frame_num * FRAME_SAMPLES];
            for (int ch = 0; ch < channels; ch++) {
                uint8_t* frame = &src_block[frame_num * frame_size];

                memset(coefs, 0, FRAME_SAMPLES * sizeof(float));
                unpack_frame(frame, coefs, steps_size, NULL, bitrate_index);
                transform_frame(NULL, coefs, coefs, NULL, fft_buf);

                int interleave = frame_num * FRAME_SAMPLES;
                for (int i = 0; i < FRAME_SAMPLES; i++) {
                    coefs[i] *= OVERLAP_WINDOW[i];
                    coefs[512 + i] *= OVERLAP_WINDOW[511 - i];
                    dst_track[i * channels + ch] = coefs[i] + prev[interleave + i];
                }

                // save overlapped samples for next
                memcpy(&prev[interleave], &coefs[512], FRAME_SAMPLES * sizeof(float));

                frame_num++;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// API (not part of original code)

struct ka1a_handle_t {
    // config
    int bitrate_mode;
    int channels;
    int tracks;

    // state
    bool setup_flag;        // next frame will be used as setup and won't output samples
    float temp[1024 * 2];   // fft + spectrum coefs buf
    float* prev;            // at least samples * channels * tracks
};

ka1a_handle_t* ka1a_init(int bitrate_mode, int channels, int tracks) {

    int bitrate_index = bitrate_mode + BITRATE_INDEX_MODIFIER;
    if (bitrate_index < 0 || bitrate_index >= MAX_BITRATES)
        return NULL;

    if (channels * tracks <= 0 || channels * tracks > MAX_CHANNELS_TRACKS)
        return NULL;

    ka1a_handle_t* ctx = calloc(1, sizeof(ka1a_handle_t));
    if (!ctx) goto fail;

    ctx->prev = calloc(1, FRAME_SAMPLES * channels * tracks * sizeof(float));
    if (!ctx) goto fail;

    ctx->bitrate_mode = bitrate_mode;
    ctx->channels = channels;
    ctx->tracks = tracks;

    ka1a_reset(ctx);

    return ctx;
fail:
    ka1a_free(ctx);
    return NULL;
}

void ka1a_free(ka1a_handle_t* ctx) {
    if (!ctx)
        return;

    free(ctx->prev);
    free(ctx);
}

void ka1a_reset(ka1a_handle_t* ctx) {
    if (!ctx)
        return;

    ctx->setup_flag = true;
    // no need to reset buffers as on next decode frame will be used to setup them.
}

int ka1a_decode(ka1a_handle_t* ctx, unsigned char* src, float* dst) {
    if (!ctx)
        return -1;
    
    decode_frame(src, ctx->tracks, ctx->channels, dst, ctx->bitrate_mode, ctx->setup_flag, ctx->prev, ctx->temp);
    
    if (ctx->setup_flag) {
        ctx->setup_flag = false;
        return 0;
    }

    return FRAME_SAMPLES;
}

int ka1a_get_frame_size(ka1a_handle_t* ctx) {
    if (!ctx)
        return 0;
    return get_frame_size(ctx->bitrate_mode);
}
