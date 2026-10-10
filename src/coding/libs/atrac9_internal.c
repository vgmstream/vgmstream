#include <math.h>
#include <string.h>
#include <stdint.h>
#include "atrac9_dec.h"
#include "atrac9_internal.h"
#include "atrac9_internal_data.h"


//-----------------------------------------------------------------------------
// TRANSFORM: DATA

// double approximation of (10 ^ (-dB / 20)); libs (also double) seem to use slightly different values
#define BEX_GAIN_MINUS_3DB   0.707945784
#define BEX_GAIN_MINUS_6DB   0.501187233
#define BEX_GAIN_MINUS_9DB   0.354813389

// sqrt(2) as float (0x3FB504F3)
#define AT9_SQRT2 1.4142135f


//-----------------------------------------------------------------------------
// UNPACK + TRANSFORM: BAND EXTENSION

/* this is called in transform and unpacking, though uses BEX (transform) tables */
// OG: set_bex_info
static int bex_get_info(int* p_band_count, int* p_group_b_qu, int* p_group_c_qu, int qu_count) {
    int band_count, group_b_qu, group_c_qu, res;

    int group_index = qu_count - 13;
    if (group_index < 0 || group_index > 7) {
        band_count = 0;
        group_b_qu = qu_count;
        group_c_qu = qu_count;
        res = 0;
    }
    else {
        const at9_bex_group_t* group = &bex_group_table[qu_count - 13];
        band_count = group->band_count;
        group_b_qu = group->group_b_qu;
        group_c_qu = group->group_c_qu;
        res = 1;
    }

    if (p_band_count)
        *p_band_count = band_count; // 'ex_bb_off'
    if (p_group_b_qu)
        *p_group_b_qu = group_b_qu; // 'rep1_ed_qu'
    if (p_group_c_qu)
        *p_group_c_qu = group_c_qu; // 'gain_man'
    return res;
}


//-----------------------------------------------------------------------------
// TRANSFORM: BAND EXTENSION

/* band extension is only allowed for 48kHz, mono/stereo and low bitrates. */
// OG: at9_check_bex_settings
static int check_bex_settings(int profile_index, int channel_config_index, int frame_size, int bands) {
    // OG does some odd validation bit-mixing before checking results, no idea

    if (profile_index != 7)
        return AT9_ERROR_TRANSFORM;

    int channels;
    if (channel_config_index == 0)
        channels = 1;
    else if (channel_config_index == 1 || channel_config_index == 2)
        channels = 2;
    else
        return AT9_ERROR_TRANSFORM;

    if ((unsigned)(frame_size / channels - 16) > 32)
        return AT9_ERROR_TRANSFORM;

    if ((unsigned)(bands - 5) > 5)
        return AT9_ERROR_TRANSFORM;

    return AT9_OK;
}

/* validates the block's band extension settings, inline'd in OG */
static inline int validate_bex_settings(at9_core_t* core, at9_block_t* block) {

    // skip modes that OG doesn't know, as the check would always fail for them
    if (!block->bex_enabled || !block->ext_enabled || is_high_sample_rate(core))
        return AT9_OK;

    int frame_size = core->superframe_size / core->frames_per_superframe;

    int bands;
    for (bands = 0; bands < 19; bands++) {
        if (band_to_qu_count_table[bands] == block->qu_count)
            break;
    }

    return check_bex_settings(core->profile_index, core->channel_config_index, frame_size, bands);
}

/* 16-bit xorshift-like noise in -1.0..1.0 */
// OG: make_uniform_random
static void bex_make_noise(float* spectrum, int count, const uint8_t* sf_indexes, uint16_t* rng) {

    // seeded once from scalefactors (0 on init)
    if (rng[0] == 0 && rng[1] == 0 && rng[2] == 0 && rng[3] == 0) {
        uint16_t seed = 543 * (sf_indexes[8] + sf_indexes[12] + sf_indexes[15] + 1);
        uint16_t start = (-19859 * (seed ^ (seed >> 14)));
        for (int i = 0; i < 4; i++) {
            rng[i] = start + i;
        }
    }

    if (count <= 0)
        return;

    uint16_t r0 = rng[0];
    uint16_t r1 = rng[1];
    uint16_t r2 = rng[2];
    uint16_t r3 = rng[3];
    for (int i = 0; i < count; i++) {
        uint16_t temp = (uint16_t)(r0 ^ (r0 << 5));
        uint16_t value = (uint16_t)(temp ^ r3 ^ (r3 >> 9) ^ (temp >> 4));

        spectrum[i] = ((double)value / 65535.0 + (double)value / 65535.0 - 1.0);

        r0 = r1;
        r1 = r2;
        r2 = r3;
        r3 = value;
    }
    rng[0] = r0;
    rng[1] = r1;
    rng[2] = r2;
    rng[3] = r3;
}

/* (not in OG) */
static inline void bex_mirror(float* spectrum, int start, int end) {
    for (int i = 0; i < end - start; i++) {
        spectrum[start + i] = spectrum[start - 1 - i];
    }
}

/* (not in OG) */
static inline void bex_scale(float* spectrum, int start, int end, double gain) {
    for (int i = start; i < end; i++) {
        spectrum[i] = (float)(spectrum[i] * gain);
    }
}

/* (not in OG) */
static inline void bex_scale_noise(float* spectrum, int end_qu, int qu_count, double* scales) {
    for (int i = 0; i < end_qu - qu_count; i++) {
        int start = qu_coef_start_table[qu_count + i];
        int end = qu_coef_start_table[qu_count + i + 1];
        bex_scale(spectrum, start, end, scales[i]);
    }
}

/* makes high spectrum coefs by mirroring lower spectrum or making noise + scaling */
// OG: bex_synthesis_mdct
static int apply_band_extension(at9_channel_t* channel, at9_block_t* block, at9_channel_temp_t* channel_temp) {
    double scales[8] = {0};

    // setup
    int qu_count = block->qu_count;
    int band_count, group_b_qu, group_c_qu;
    if (!bex_get_info(&band_count, &group_b_qu, &group_c_qu, qu_count))
        return AT9_OK;
    int end_qu = band_count == 0 ? 22 : group_c_qu;

    // coef regions (0..A= original, A..B/B..C/C..End = BEX)
    int group_a_coef = qu_coef_start_table[qu_count];
    int group_b_coef = qu_coef_start_table[group_b_qu];
    int group_c_coef = qu_coef_start_table[group_c_qu];
    int end_coef = qu_coef_start_table[end_qu];


    const uint8_t* sf_indexes = channel_temp->sf_indexes;
    int method = channel->bex.method;

    // setup scales
    if (method == 1) {
        // get scales from scalefactors
        for (int i = 0; i < end_qu - qu_count; i++) {
            int sf_index = sf_indexes[qu_count + i];
            scales[i] = scalefactor_scale_table[sf_index];
        }
    }
    else {
        // get scales from 'VQC' codebooks
        const double* const* codebooks;
        switch (method) {
            case 0:
                switch (band_count) {
                    case 3: codebooks = bex_normal_codebooks_band3; break;
                    case 4: codebooks = bex_normal_codebooks_band4; break;
                    case 5: codebooks = bex_normal_codebooks_band5; break;
                    default: return AT9_ERROR_TRANSFORM;
                }
                break;
            case 2: codebooks = bex_tone_codebooks; break;
            case 3: codebooks = bex_curve_codebooks; break;
            case 4: codebooks = bex_extra_codebooks; break;
            default: return AT9_ERROR_TRANSFORM;
        }

        int scale_count = 0;
        for (int i = 0; i < channel->bex.value_count; i++) {
            int dimensions = bex_value_dimension_table[method][band_count][i];
            if (dimensions == 0)
                continue;

            int index = channel->bex.values[i];
            const double* dimension_scales = &codebooks[i][dimensions * index];
            for (int j = 0; j < dimensions; j++) {
                scales[scale_count] = dimension_scales[j];
                scale_count += 1;
            }
        }
        if (method == 0) {
            int sf_index = sf_indexes[qu_count];
            scales[scale_count] = scalefactor_scale_table[sf_index];
        }
    }


    float* spectrum = channel_temp->spectrum;

    // fill spectrum + scale (this is kind of fused with the above in OG)
    if (method == 0) {
        // fix BEX spectrum with mirrored and noise coefs
        bex_mirror(spectrum, group_a_coef, group_b_coef);
        int noise_start = qu_coef_start_table[end_qu - 1];
        int range_count = end_coef - noise_start;
        bex_make_noise(&spectrum[noise_start], range_count, sf_indexes, channel->bex.rng);

        // 'normal' gains
        bex_scale_noise(spectrum, end_qu, qu_count, scales);
    }
    else if (method == 1) {
        // fill BEX spectrum with noise coefs
        int noise_start = group_a_coef;
        int range_count = end_coef - noise_start;
        bex_make_noise(&spectrum[noise_start], range_count, sf_indexes, channel->bex.rng);

        // 'scalefactor' gains
        bex_scale_noise(spectrum, end_qu, qu_count, scales);
    }
    else {
        // fill BEX spectrum with mirrored coefs
        bex_mirror(spectrum, group_a_coef, group_b_coef);
        bex_mirror(spectrum, group_b_coef, group_c_coef);
        bex_mirror(spectrum, group_c_coef, end_coef);

        if (method == 2) {
            // "tone" gains
            bex_scale(spectrum, group_a_coef, group_b_coef, scales[0]);
            bex_scale(spectrum, group_b_coef, group_c_coef, scales[1]);
        }
        else if (method == 3) {
            // "curve" gains
            double rate = pow(2.0, scales[1]);
            double scale = scales[0] * rate;
            for (int i = group_a_coef; i < end_coef; i++) {
                spectrum[i] = (float)(spectrum[i] * scale);
                scale = scale * rate;
            }
        }
        else {
            // "extra" gains
            bex_scale(spectrum, group_a_coef, group_b_coef, BEX_GAIN_MINUS_3DB * scales[0]);
            bex_scale(spectrum, group_b_coef, group_c_coef, BEX_GAIN_MINUS_6DB * scales[0]);
            bex_scale(spectrum, group_c_coef, end_coef, BEX_GAIN_MINUS_9DB * scales[0]);
        }
    }

    return AT9_OK;
}


//-----------------------------------------------------------------------------
// TRANSFORM: IMDCT

/* Sony's IMDCT, despite the debug symbols it may actually not be a Chebyshev algorithm? (radix-2 DCT-IV?).
 * OG has 3 copies that only seem to differ in tables and unrolled loops. */
static void imdct_transform(float* spectrum, float* samples, float* overlap, int n,
        const float* twiddles, const float* post_twiddles, const float* post_scales, const float* window) {
    int half = n / 2;
    int twiddle_last = half - 1;
    int reorder_step = 256 / n;

    // pre-process
    for (int i = 0; i < half; i++) {
        spectrum[i] = spectrum[i] - spectrum[n - 1 - i];
    }
    for (int i = 0; i < half; i++) {
        float sub_coef = spectrum[half + i] * AT9_SQRT2;
        float main_coef = spectrum[i];
        spectrum[half + i] = main_coef - sub_coef;
        spectrum[i] = main_coef + sub_coef;
    }

    // twiddle/butterfly block steps
    for (int size = half / 2; size >= 2; size >>= 1) {
        int blocks = half / size;
        float* sp_block = spectrum;

        for (int b = 0; b < blocks; b++) {
            float twiddle = twiddles[twiddle_last - size / 2 - b * size];

            for (int i = 0; i < size; i++) {
                sp_block[i] = sp_block[i] - sp_block[2 * size - 1 - i];
            }

            for (int i = 0; i < size; i += 2) {
                float a0 = sp_block[i + 0];
                float a1 = sp_block[i + 1];
                float b0 = sp_block[size + i + 0] * twiddle;
                float b1 = sp_block[size + i + 1] * twiddle;
                sp_block[size + i + 0] = a0 - b0;
                sp_block[size + i + 1] = a1 - b1;
                sp_block[i + 0] = b0 + a0;
                sp_block[i + 1] = b1 + a1;
            }

            sp_block += 2 * size;
        }
    }

    // post-twiddles + window + overlap
    for (int i = 0; i < half; i++) {
        float odd = spectrum[2 * i + 1];
        float diff = spectrum[2 * i + 0] - odd;
        float temp = odd * post_twiddles[i];
        float weight = window[i];
        float result0 = (diff - temp) * post_scales[i];
        float result1 = (temp + diff) * post_scales[half + i];

        spectrum[2 * i + 0] = overlap[i] - (weight * result0);
        spectrum[2 * i + 1] = (weight * overlap[i]) + result0;
        overlap[i] = result1;
    }

    // reorder
    for (int i = 0; i < half; i++) {
        int pos = imdct_mix_table[i * reorder_step];
        samples[half - 1 - pos] = spectrum[2 * i + 0];
        samples[half + pos] = spectrum[2 * i + 1];
    }
}

// OG: chebyshev
static void imdct_256(float* spectrum, float* samples, float* overlap) {
    imdct_transform(spectrum, samples, overlap, 256, imdct_twiddles_256, imdct_post_twiddles_256, imdct_post_scales_256, imdct_window_256);
}

// OG: chebyshev_128
static void imdct_128(float* spectrum, float* samples, float* overlap) {
    imdct_transform(spectrum, samples, overlap, 128, imdct_twiddles_128, imdct_post_twiddles_128, imdct_post_scales_128, imdct_window_128);
}

// OG: chebyshev_64
static void imdct_64(float* spectrum, float* samples, float* overlap) {
    imdct_transform(spectrum, samples, overlap, 64, imdct_twiddles_64, imdct_post_twiddles_64, imdct_post_scales_64, imdct_window_64);
}


//-----------------------------------------------------------------------------
// TRANSFORM: SCALEFACTORS

// OG: denormalize_at9
static void apply_scalefactors(at9_block_t* block, at9_channel_temp_t* channel_temp) {
    const uint16_t* coef_start_table = qu_coef_start_table;

    // V3: vibration blocks have their own QU layout (2 coefs per unit)
    if (block->type == AT9_BLOCK_VIBRATION)
        coef_start_table = qu_coef_start_table_vib;

    for (int qu = 0; qu < block->qu_count; qu++) {
        int sf_index = channel_temp->sf_indexes[qu];
        float scale = scalefactor_scale_table[sf_index];

        for (int i = coef_start_table[qu]; i < coef_start_table[qu + 1]; i++) {
            channel_temp->spectrum[i] *= scale;
        }
    }
}


//-----------------------------------------------------------------------------
// TRANSFORM: SYNTHESIS

// OG: s2t_at9
int transform_block(at9_core_t* core, at9_block_t* block, at9_channel_temp_t* channel_temps) {
    int res;

    for (int ch = 0; ch < block->channel_count; ch++) {
        at9_channel_t* channel = block->channels[ch];
        at9_channel_temp_t* channel_temp = &channel_temps[ch];

        // OG does this during unpack_block, but that feels odd
        apply_scalefactors(block, channel_temp);

        if (block->bex_enabled && block->ext_enabled) {
            res = apply_band_extension(channel, block, channel_temp);
            if (res < 0)
                return AT9_ERROR_TRANSFORM;
        }

        // OG selects by profile (7: 256, 4: 128, others 64) as it only allows 12/24/48kHz
        switch (core->frame_samples) {
            case 256: imdct_256(channel_temp->spectrum, channel_temp->samples, channel->overlap); break;
            case 128: imdct_128(channel_temp->spectrum, channel_temp->samples, channel->overlap); break;
            default:  imdct_64(channel_temp->spectrum, channel_temp->samples, channel->overlap); break;
        }
    }

    // OG validates outside this function, which feels like a bug (other libs do it before), probably harmless
    res = validate_bex_settings(core, block);
    if (res < 0)
        return res;

    return AT9_OK;
}


//-----------------------------------------------------------------------------
// UNPACK: BITREADER

/* OG expects and validates buf of 0x800. It also only reads <= 16 bits so probably faster though. */
// OG: readNBits2 (2=short?)
static inline int read_bits(bitstream_t* is, int bits) {
    return (int)bm_read(is, bits);
}

static inline int sign_extend(int value, int bits) {
    if (value & (1 << (bits - 1)))
        value = (int)((unsigned)value | (~0u << bits));
    return value;
}


//-----------------------------------------------------------------------------
// UNPACK: HUFFMAN

/* fills every code's (max_length bits) lookup entries with its symbol */
// OG: make_dectbl_at9
static void make_huffman_lookup(at9_codebook_t* codebook, uint8_t* lookup) {
    int max_length = codebook->max_length;

    if (!codebook->max_symbol)
        return;

    for (int symbol = 0; symbol <= codebook->max_symbol; symbol++) {
        int length = codebook->codes[symbol].length;
        if (!length)
            continue;

        int start = codebook->codes[symbol].code << (max_length - length);
        int count = 1 << (max_length - length);
        for (int i = 0; i < count; i++) {
            lookup[start + i] = symbol;
        }
    }
}

/* Rebuilt on every init, like OG. Not race-free but safe enough since always writes the same values. */
// OG: init_sf_huffman_dec_table_at9 + init_sp_huffman_dec_table_at9
void init_huffman_tables(void) {

    // setup scalefactor codebooks
    for (int i = 0; i < 6; i++) {
        make_huffman_lookup(&scalefactor_unsigned_codebooks[i], scalefactor_unsigned_lookups[i]);
        scalefactor_unsigned_codebooks[i].lookup = scalefactor_unsigned_lookups[i];
    }

    for (int i = 0; i < 4; i++) {
        make_huffman_lookup(&scalefactor_signed_codebooks[i], scalefactor_signed_lookups[i]);
        scalefactor_signed_codebooks[i].lookup = scalefactor_signed_lookups[i];
    }

    // setup spectrum codebooks
    for (int set = 0; set < 2; set++) {
        for (int wl = 0; wl < 6; wl++) {
            for (int size = 0; size < 4; size++) {
                at9_codebook_t* codebook = &spectrum_codebooks[set][wl][size];
                make_huffman_lookup(codebook, spectrum_lookups[set][wl][size]);
                codebook->lookup = spectrum_lookups[set][wl][size];
            }
        }
    }

    // V3 init
    for (int wl = 0; wl < 6; wl++) {
        make_huffman_lookup(&spectrum_codebooks_vib[wl], spectrum_lookups_vib[wl]);
        spectrum_codebooks_vib[wl].lookup = spectrum_lookups_vib[wl];
    }
}

/* peek symbol + read actual length */
// OG: read_varlencode_dectbl_at9
static int read_huffman_symbol(bitstream_t* is, const at9_codebook_t* codebook) {
    // Files regularly peek over superframe_size, but OG assumes buf sizes are 0x800 (superframe max) so
    // they can't overread (would need calloc too?). bm_peek here allows peeking near EOB.

    uint32_t code = bm_peek(is, codebook->max_length);

    int symbol = codebook->lookup[code];
    uint16_t length = codebook->codes[symbol].length;

    bm_skip(is, length);

    return symbol;
}

/* Unpacks N values per huffman symbol. */
// OG: hc_ungrp_at9
static void ungroup_huffman_codes(float* spectrum, float step, int qu_coefs, int value_count, int value_bits, int value_mask, const int16_t* symbols) {
    int groups = qu_coefs / value_count;

    for (int i = 0; i < groups; i++) {
        int value = symbols[i];
        for (int j = value_count - 1; j >= 0; j--) {
            int coef = sign_extend(value & value_mask, value_bits);

            spectrum[i * value_count + j] = (float)coef * step;
            value >>= value_bits;
        }
    }
}


//-----------------------------------------------------------------------------
// UNPACK: SPECTRUM

// OG: clear_mdspec_at9
static inline void clear_spectrum(at9_channel_temp_t* channel_temp, int coefs) {
    memset(channel_temp->spectrum, 0, coefs * sizeof(float));
}

/* fine coefs, added over coarse ones */
// OG: unpack_component_at9
static void unpack_spectrum_fine(bitstream_t* is, at9_channel_temp_t* channel_temp, int qu_count) {
    for (int qu = 0; qu < qu_count; qu++) {
        int cl_index = channel_temp->cl_indexes[qu];
        if (cl_index <= 0)
            continue;

        int word_length = channel_temp->wl_indexes[qu];
        float icqsf = quantizer_scale_table[cl_index] * fine_scale_table[word_length];
        int bits = word_length_table[cl_index];

        for (int i = qu_coef_start_table[qu]; i < qu_coef_start_table[qu + 1]; i++) {
            int value = read_bits(is, bits);
            int coef = sign_extend(value, bits);
            channel_temp->spectrum[i] = (float)coef * icqsf + channel_temp->spectrum[i];
        }
    }
}

// OG: get_idhf_at9
static int get_qu_size_index(int qu_coefs) {
    switch (qu_coefs) {
        case 2: return 0;
        case 4: return 1;
        case 8: return 2;
        default: return 3;
    }
}

/* read spectrum coefs */
// OG: unpack_spec_at9
static void unpack_spectrum(at9_core_t* core, bitstream_t* is, at9_channel_temp_t* channel_temp, int qu_count) {
    int16_t symbols[16];

    clear_spectrum(channel_temp, AT9_MAX_COEFS);

    for (int qu = 0; qu < qu_count; qu++) {
        int wl_index = channel_temp->wl_indexes[qu];
        float iqf = quantizer_scale_table[wl_index];
        int coef_start = qu_coef_start_table[qu];
        int coef_end = qu_coef_start_table[qu + 1];

        if (wl_index <= 6 && !is_high_sample_rate(core)) {
            int qu_coefs = qu_coef_count_table[qu];
            int size_index = get_qu_size_index(qu_coefs);
            int codebook_set = channel_temp->codebook_sets[qu];
            const at9_codebook_t* codebook = &spectrum_codebooks[codebook_set][wl_index - 1][size_index];

            int groups = qu_coefs >> codebook->value_shift;
            for (int i = 0; i < groups; i++) {
                symbols[i] = read_huffman_symbol(is, codebook);
            }

            float* spectrum_group = &channel_temp->spectrum[coef_start];
            ungroup_huffman_codes(spectrum_group, iqf, qu_coefs, codebook->value_count, codebook->value_bits, codebook->value_mask, symbols);
        }
        else {
            int bits = word_length_table[wl_index];
            for (int i = coef_start; i < coef_end; i++) {
                int value = read_bits(is, bits);
                int coef = sign_extend(value, bits);
                channel_temp->spectrum[i] = (float)coef * iqf;
            }
        }
    }
}


// OG: unpack_spec_at9_lfe
static void unpack_spectrum_lfe(bitstream_t* is, at9_channel_temp_t* channel_temp, int qu_count) {

    clear_spectrum(channel_temp, AT9_MAX_COEFS);

    for (int qu = 0; qu < qu_count; qu++) {
        int wl_index = channel_temp->wl_indexes[qu];
        float iqf = quantizer_scale_table[wl_index];

        int bits = word_length_table[wl_index];
        for (int i = qu_coef_start_table[qu]; i < qu_coef_start_table[qu + 1]; i++) {
            int value = read_bits(is, bits);
            int coef = sign_extend(value, bits);
            channel_temp->spectrum[i] = (float)coef * iqf;
        }
    }
}


/* V3: libs have this as part of unpack_spectrum() with some block_type checks, but separate it here to simplify and keep OG style */
static void unpack_spectrum_vib(at9_core_t* core, bitstream_t* is, at9_channel_temp_t* channel_temp, int qu_count) {
    int16_t symbols[16];

    clear_spectrum(channel_temp, AT9_MAX_COEFS);

    for (int qu = 0; qu < qu_count; qu++) {
        int wl_index = channel_temp->wl_indexes[qu];
        float iqf = quantizer_scale_table[wl_index];
        int coef_start = qu_coef_start_table_vib[qu];
        int coef_end = qu_coef_start_table_vib[qu + 1];

        if (wl_index <= 6 && !is_high_sample_rate(core)) {
            int qu_coefs = 2;
            const at9_codebook_t* codebook = &spectrum_codebooks_vib[wl_index - 1];

            int groups = qu_coefs >> codebook->value_shift;
            for (int i = 0; i < groups; i++) {
                symbols[i] = read_huffman_symbol(is, codebook);
            }

            float* spectrum_group = &channel_temp->spectrum[coef_start];
            ungroup_huffman_codes(spectrum_group, iqf, qu_coefs, codebook->value_count, codebook->value_bits, codebook->value_mask, symbols);
        }
        else {
            int bits = word_length_table[wl_index];
            for (int i = coef_start; i < coef_end; i++) {
                int value = read_bits(is, bits);
                int coef = sign_extend(value, bits);
                channel_temp->spectrum[i] = (float)coef * iqf;
            }
        }
    }
}


//-----------------------------------------------------------------------------
// UNPACK: BITALLOC

/* Updates word lengths ('wl', bits per coef) index arrays from scalefactors and calculated gradient */
// OG: noise_shaping_at9
static void calculate_word_lengths(at9_block_temp_t* block_temp, at9_block_t* block, at9_channel_temp_t* channel_temp) {

    int qu_lo = block_temp->gradient_qu_lo;
    int qu_hi = block_temp->gradient_qu_hi;
    int os_lo = block_temp->gradient_os_lo;
    int os_hi = block_temp->gradient_os_hi;
    int qu_range = qu_hi - qu_lo;
    int os_range = os_hi - os_lo;

    // gradient defaults before and after qu_hi (curve main point?)
    int* gradient = block_temp->gradient;
    memset(gradient, 0, sizeof(block_temp->gradient));

    for (int qu = 0; qu < qu_hi && qu < AT9_MAX_QUS; qu++) {
        gradient[qu] = os_lo;
    }
    for (int qu = qu_hi; qu < block->qu_count && qu < AT9_MAX_QUS; qu++) {
        gradient[qu] = os_hi;
    }

    // main gradient curve
    if (qu_range > 0) {
        const uint8_t* resample_curve = gradient_resample_table[qu_range - 1];

        if (os_range > 0) {
            for (int qu = qu_lo; qu < qu_hi && qu < AT9_MAX_QUS; qu++) {
                int gradient_index = resample_curve[qu - qu_lo];
                int curve_point = gradient_curve_table[gradient_index];
                gradient[qu] += ((33 * (os_range - 1) * curve_point) >> 10) + 1; //???
            }
        }
        else if (os_range < 0) {
            for (int qu = qu_lo; qu < qu_hi && qu < AT9_MAX_QUS; qu++) {
                int gradient_index = resample_curve[qu - qu_lo];
                int curve_point = gradient_curve_table[gradient_index];
                gradient[qu] += ~((33 * curve_point * ~os_range) >> 10); //???
            }
        }
    }

    const uint8_t* sf_indexes = channel_temp->sf_indexes;

    // rough word lengths, based on scalefactor and the gradient calculated above
    int* wl_indexes = channel_temp->wl_indexes; // (int since apparently during calcs they can become < 0)
    int* cl_indexes = channel_temp->cl_indexes;
    memset(wl_indexes, 0, sizeof(channel_temp->wl_indexes));
    memset(cl_indexes, 0, sizeof(channel_temp->cl_indexes));

    for (int qu = 0; qu < block->qu_count; qu++) {
        wl_indexes[qu] = sf_indexes[qu] - gradient[qu];
    }

    // fine word lenghts adjustments
    int* wl_adjust = block_temp->wl_adjust;
    memset(wl_adjust, 0, sizeof(block_temp->wl_adjust));

    for (int qu = 1; qu < block->qu_count; qu++) {
        int delta = sf_indexes[qu] - sf_indexes[qu - 1];

        // OG uses ifs + switch per deltas to sum -1 (micro optimization?)
        if (delta > 1) {
            int value = delta - 1;
            if (value > 5)
                value = 5;
            wl_adjust[qu] += value;
        }
        else if (delta < -1) {
            int value = -delta - 1;
            if (value > 5)
                value = 5;
            wl_adjust[qu - 1] += value;
        }
    }

    // tweak word lengths some more based on wl_adjust + gradient
    switch (block_temp->gradient_mode) {
        case 1:
            for (int qu = 0; qu < block->qu_count; qu++) {
                int wl_index = wl_adjust[qu] + wl_indexes[qu];
                if (wl_index > 0)
                    wl_index >>= 1;
                wl_indexes[qu] = wl_index;
            }
            break;
        case 2:
            for (int qu = 0; qu < block->qu_count; qu++) {
                int wl_index = wl_adjust[qu] + wl_indexes[qu];
                if (wl_index > 0)
                    wl_index = (3 * wl_index) >> 3;
                wl_indexes[qu] = wl_index;
            }
            break;
        case 3:
            for (int qu = 0; qu < block->qu_count; qu++) {
                int wl_index = wl_adjust[qu] + wl_indexes[qu];
                if (wl_index > 0)
                    wl_index >>= 2;
                wl_indexes[qu] = wl_index;
            }
            break;
        default:
            // apparently there is no gradient_mode 0, so wl_adjust is always calculated above
            break;
    }

    for (int qu = 0; qu < block->qu_count; qu++) {
        if (wl_indexes[qu] <= 0)
            wl_indexes[qu] = 1;
    }

    // extra fixup
    for (int qu = 0; qu < block_temp->adjust_qu_count; qu++) {
        wl_indexes[qu]++;
    }

    // word lengths over 15 go to fine coefs
    for (int qu = 0; qu < block->qu_count; qu++) {
        if (wl_indexes[qu] > 15) {
            int cl_index = wl_indexes[qu] - 15;
            if (cl_index > 15)
                cl_index = 15;
            cl_indexes[qu] = cl_index;
            wl_indexes[qu] = 15;
        }
    }
}

// OG: noise_shaping_at9_lfe
static void calculate_word_lengths_lfe(at9_block_t* block, at9_channel_temp_t* channel_temp, int band_info_absent) {
    int wl_index = band_info_absent == 0 ? 4 : 8;

    for (int qu = 0; qu < block->qu_count; qu++) {
        channel_temp->wl_indexes[qu] = wl_index;
        channel_temp->cl_indexes[qu] = 0;
    }
}

/* Selects the huffman codebook set per QU, based on scalefactor peaks. */
// OG: detect_projection_at9
static int select_codebook_sets(at9_channel_temp_t* channel_temp, int qu_count) {
    uint8_t* codebook_sets = channel_temp->codebook_sets;

    memset(codebook_sets, 0, sizeof(channel_temp->codebook_sets));
    if (qu_count <= 1)
        return 0;

    int changes = 0;
    int average = 0;
    int last = qu_count - 1;
    const uint8_t* sf_indexes = channel_temp->sf_indexes;

    if (qu_count > 12) {
        int sum = 0;
        for (int qu = 0; qu < 12; qu++) {
            sum += sf_indexes[qu];
        }
        average = (2731 * sum + 0x4000) >> 15; // sum / 12, rounded
    }

    for (int qu = 1; qu < last; qu++) {
        if (qu_coef_count_table[qu] > 3) {
            int prev = sf_indexes[qu - 1];
            int next = sf_indexes[qu + 1];
            int min = prev <= next ? prev : next;
            if (sf_indexes[qu] - min > 2 || 2 * sf_indexes[qu] - (next + prev) > 2) {
                codebook_sets[qu] = 1;
                changes++;
            }
        }
    }

    if (sf_indexes[last] - sf_indexes[last - 1] > 2) {
        codebook_sets[last] = 1;
        changes++;
    }

    for (int qu = 12; qu < last; qu++) {
        if (!codebook_sets[qu]) {
            int prev = sf_indexes[qu - 1];
            int next = sf_indexes[qu + 1];
            int min = prev <= next ? prev : next;
            if (sf_indexes[qu] - min > 1 && sf_indexes[qu] >= average - (qu_coef_count_table[qu] == 16)) {
                codebook_sets[qu] = 1;
                changes++;
            }
        }
    }

    if (qu_count > 12 && !codebook_sets[last]
            && sf_indexes[last] - sf_indexes[last - 1] > 1
            && sf_indexes[last] >= average - (qu_coef_count_table[last] == 16)) {
        codebook_sets[last] = 1;
        changes++;
    }

    return changes;
}


//-----------------------------------------------------------------------------
// UNPACK: SCALEFACTORS

// OG: unpack_idsf_0_at9
static void unpack_scalefactors_weights(bitstream_t* is, int qu_count, uint8_t* sf_indexes) {
    int wt_index = read_bits(is, 3);
    int sf_offset = read_bits(is, 5);
    int index = read_bits(is, 2);

    const uint8_t* weights = scalefactor_weight_table[wt_index];
    const at9_codebook_t* codebook = &scalefactor_unsigned_codebooks[index + 2];

    sf_indexes[0] = read_bits(is, index + 3);
    for (int qu = 0; qu < qu_count - 1; qu++) {
        int delta = read_huffman_symbol(is, codebook);

        sf_indexes[qu + 1] = codebook->value_mask & (sf_indexes[qu] + delta);
        sf_indexes[qu] += sf_offset - weights[qu];
    }
    sf_indexes[qu_count - 1] += sf_offset - weights[qu_count - 1];
}

// inline'd unpack_idsf_1_at9?
static void unpack_scalefactors_distance_base(bitstream_t* is, int qu_count, uint8_t* sf_indexes, const uint8_t* sf_base) {
    int cb_index = read_bits(is, 2);

    const at9_codebook_t* codebook = &scalefactor_signed_codebooks[cb_index];

    for (int qu = 0; qu < qu_count; qu++) {
        int symbol = read_huffman_symbol(is, codebook);
        int distance = sign_extend(symbol, codebook->value_bits);

        sf_indexes[qu] = (sf_base[qu] + distance) & 31;
    }
}

// inline'd unpack_idsf_1_at9?
static void unpack_scalefactors_offset(bitstream_t* is, int qu_count, uint8_t* sf_indexes) {
    int bits = read_bits(is, 2) + 2;

    if (bits <= 4) {
        int offset = read_bits(is, 5);
        for (int qu = 0; qu < qu_count; qu++) {
            int value = read_bits(is, bits);

            sf_indexes[qu] = offset + value;
        }
    }
    else { // bits == 5
        for (int qu = 0; qu < qu_count; qu++) {
            sf_indexes[qu] = read_bits(is, bits);
        }
    }
}

// inline'd unpack_idsf_2_at9?
static void unpack_scalefactors_delta_base(bitstream_t* is, int qu_count, uint8_t* sf_indexes, const uint8_t* sf_base) {
    int offset = read_bits(is, 5) - 16;
    int index = read_bits(is, 2);

    const at9_codebook_t* codebook = &scalefactor_unsigned_codebooks[index];

    sf_indexes[0] = read_bits(is, index + 1); // always
    if (qu_count <= 0)
        return;

    for (int qu = 0; qu < qu_count - 1; qu++) {
        int delta = read_huffman_symbol(is, codebook);

        sf_indexes[qu + 1] = codebook->value_mask & (sf_indexes[qu] + delta);
    }

    for (int qu = 0; qu < qu_count; qu++) {
        int sf_index_base = sf_base[qu];

        sf_indexes[qu] += offset + sf_index_base;
    }
}

// OG: unpack_idsf_4_at9
static void unpack_scalefactors_distance_prev(bitstream_t* is, int qu_count, int qu_prev, uint8_t* sf_indexes, const uint8_t* sf_prev) {

    int base_qu = qu_prev < qu_count ? qu_prev : qu_count;

    if (base_qu > 0)
        unpack_scalefactors_distance_base(is, base_qu, sf_indexes, sf_prev);
    else
        read_bits(is, 2); // cb_index from _distance, which OG reads first (inline'd?)

    for (int qu = base_qu; qu < qu_count; qu++) {
        sf_indexes[qu] = read_bits(is, 5);
    }
}

static void unpack_scalefactors_delta_prev(bitstream_t* is, int qu_count, int qu_prev, uint8_t* sf_indexes, const uint8_t* sf_prev) {

    int base_qu = qu_prev < qu_count ? qu_prev : qu_count;

    unpack_scalefactors_delta_base(is, base_qu, sf_indexes, sf_prev);

    for (int qu = base_qu; qu < qu_count; qu++) {
        sf_indexes[qu] = read_bits(is, 5);
    }
}

/* packed scalefactor indexes packed in various modes */
// OG: unpack_idsf_at9
static int unpack_scalefactor_indexes(bitstream_t* is, at9_block_temp_t* block_temp, at9_block_t* block, at9_channel_t* channel, at9_channel_temp_t* channel_temp, int ch, int frame_id) {
    int qu_count = block->bex_qu_count; // odd but true

    memset(channel_temp->sf_indexes, 0, sizeof(channel_temp->sf_indexes));

    uint8_t* sf_indexes = channel_temp->sf_indexes;
    const uint8_t* sf_prev = channel->sf_indexes_prev;

    int mode = read_bits(is, 2);
    if (ch == 1) {
        // channel 1 may reuse channel 0's scalefactors
        const uint8_t* sf_base = (channel_temp - 1)->sf_indexes; //TODO: improve ugly array accesses

        switch (mode) {
            case 0:
                unpack_scalefactors_weights(is, qu_count, sf_indexes);
                break;

            case 1:
                unpack_scalefactors_distance_base(is, qu_count, sf_indexes, sf_base);
                break;

            case 2:
                unpack_scalefactors_delta_base(is, qu_count, sf_indexes, sf_base);
                break;

            case 3:
            default:
                if (frame_id == 0) // no prev
                    return AT9_ERROR_UNPACK;
                unpack_scalefactors_distance_prev(is, qu_count, block->qu_count_prev, sf_indexes, sf_prev);
                break;
        }
    }
    else {
        switch (mode) {
            case 0:
                unpack_scalefactors_weights(is, qu_count, sf_indexes);
                break;

            case 1:
                unpack_scalefactors_offset(is, qu_count, sf_indexes);
                break;

            case 2:
                if (frame_id == 0) // no prev
                    return AT9_ERROR_UNPACK;
                unpack_scalefactors_distance_prev(is, qu_count, block->qu_count_prev, sf_indexes, sf_prev);
                break;

            case 3:
            default:
                if (frame_id == 0) // no prev
                    return AT9_ERROR_UNPACK;
                unpack_scalefactors_delta_prev(is, qu_count, block->qu_count_prev, sf_indexes, sf_prev);
                break;
        }
    }

    // validate indexes
    for (int qu = 0; qu < AT9_MAX_QUS; qu++) {
        int sf_index = sf_indexes[qu];
        if (sf_index < 0 || sf_index >= 32)
            return AT9_ERROR_UNPACK;
    }

    for (int qu = 0; qu < AT9_MAX_QUS; qu++) {
        channel->sf_indexes_prev[qu] = sf_indexes[qu];
    }

    return AT9_OK;
}

// OG: unpack_idsf_at9_lfe
static void unpack_scalefactor_indexes_lfe(bitstream_t* is, at9_block_t* block, at9_channel_temp_t* channel_temp) {

    memset(channel_temp->sf_indexes, 0, sizeof(channel_temp->sf_indexes));

    for (int qu = 0; qu < block->qu_count; qu++) {
        channel_temp->sf_indexes[qu] = read_bits(is, 5);
    }
}


//-----------------------------------------------------------------------------
// UNPACK: BLOCK PARAMS

/* QU counts + band extension (bex) flag */
// OG: unpack_bandinfo_at9
static int unpack_band_info(at9_core_t* core, bitstream_t* is, at9_block_t* block) {
    int min_bands = is_high_sample_rate(core) ? 1 : 3;

    int bands = read_bits(is, 4) + min_bands;
    if (bands > max_bands_table[core->profile_index])
        return AT9_ERROR_UNPACK;

    block->qu_count = band_to_qu_count_table[bands];
    block->stereo_qu_count = block->qu_count;

    if (block->type == AT9_BLOCK_STEREO) {
        int stereo_bands = read_bits(is, 4) + min_bands;
        if (bands < stereo_bands)
            return AT9_ERROR_UNPACK;
        block->stereo_qu_count = band_to_qu_count_table[stereo_bands];
    }

    block->bex_enabled = read_bits(is, 1);
    if (block->bex_enabled) {
        int bex_bands = read_bits(is, 4) + min_bands;
        if (bands > bex_bands)
            return AT9_ERROR_UNPACK;

        block->bex_qu_count = band_to_qu_count_table[bex_bands];
    }
    else {
        block->bex_qu_count = block->qu_count; // ??? needed for blocks without BEX when reading SFs it seems
    }

    return AT9_OK;
}

/* read gradient curve that configures bitalloc */
// OG: unpack_gradient_at9
static int unpack_gradient(bitstream_t* is, at9_block_temp_t* block_temp, at9_block_t* block) {

    block_temp->gradient_mode = read_bits(is, 2);
    if (block_temp->gradient_mode) {
        block_temp->gradient_qu_hi = 31;
        block_temp->gradient_os_hi = 31;
        block_temp->gradient_qu_lo = read_bits(is, 5);
        block_temp->gradient_os_lo = read_bits(is, 5);
    }
    else {
        block_temp->gradient_qu_lo = read_bits(is, 6);
        block_temp->gradient_qu_hi = read_bits(is, 6) + 1;
        block_temp->gradient_os_lo = read_bits(is, 5);
        block_temp->gradient_os_hi = read_bits(is, 5);
    }

    block_temp->adjust_qu_count = read_bits(is, 4);
    if (block_temp->adjust_qu_count > block->qu_count)
        return AT9_ERROR_UNPACK;
    if (block_temp->gradient_qu_lo > 47)
        return AT9_ERROR_UNPACK;
    if (block_temp->gradient_qu_hi < 1 || block_temp->gradient_qu_hi > 48)
        return AT9_ERROR_UNPACK;
    if (block_temp->gradient_qu_lo > block_temp->gradient_qu_hi)
        return AT9_ERROR_UNPACK;

    return AT9_OK;
}

/* consume BEX VQs up to allowed bits */
static inline int unpack_bex_values(bitstream_t* is, at9_bex_t* bex, int band_count, int* p_bits_left) {
    for (int i = 0; i < bex->value_count; i++) {
        int bits = bex_value_bits_table[bex->method][band_count][i];
        if (bits > *p_bits_left)
            return AT9_ERROR_UNPACK;

        bex->values[i] = read_bits(is, bits);
        *p_bits_left -= bits;
    }
    return AT9_OK;
}

/* stereo and band extension params */
// OG: unpack_depend_data_at9
static int unpack_extra_params(bitstream_t* is, at9_block_temp_t* block_temp, at9_block_t* block) {
    int res;

    int band_count = 0;

    block_temp->main_channel = 0;
    if (block->type == AT9_BLOCK_STEREO) {
        block_temp->main_channel = read_bits(is, 1);
        int has_stereo_signs = read_bits(is, 1);
        if (has_stereo_signs) {
            for (int qu = block->stereo_qu_count; qu < block->qu_count; qu++) {
                block_temp->stereo_signs[qu] = read_bits(is, 1);
            }
        }
        else {
            memset(block_temp->stereo_signs, 0, sizeof(block_temp->stereo_signs));
        }
    }

    if (block->bex_enabled) {
        bex_get_info(&band_count, NULL, NULL, block->qu_count);

        int method_high = read_bits(is, 1); // read yet unused in mono?
        if (block->type == AT9_BLOCK_STEREO) {
            int method_low = read_bits(is, 1);
            at9_bex_t* bex = &block->channels[1]->bex; // R BEX info

            if (band_count > 2)
                bex->method = ((method_high << 1) | method_low);
            else
                bex->method = 4;
            bex->value_count = bex_value_count_table[bex->method][band_count];
        }
    }

    block->ext_enabled = read_bits(is, 1);
    if (!block->ext_enabled)
        return AT9_OK;

    int method = read_bits(is, 2);
    int bits_left = read_bits(is, 5);

    if (block->bex_enabled) {
        at9_bex_t* bex = &block->channels[0]->bex; // L BEX info

        if (band_count > 2)
            bex->method = method;
        else
            bex->method = 4;

        // keeps prev values (and value count)
        if (bits_left <= 0) {

            // V3: extra validation to avoid bad config (this possibly fixes buggy cases of wrong shared BEX values between channels)
            if (bex->method != 1)
                return AT9_ERROR_UNPACK;
            if (block->type == AT9_BLOCK_STEREO && block->channels[1]->bex.method != 1)
                return AT9_ERROR_UNPACK;

            return AT9_OK;
        }

        bex->value_count = bex_value_count_table[bex->method][band_count];

        res = unpack_bex_values(is, bex, band_count, &bits_left);
        if (res < 0) return res;

        if (block->type == AT9_BLOCK_STEREO) {
            bex = &block->channels[1]->bex; // R BEX info
            if (bex->value_count > 0) {
                res = unpack_bex_values(is, bex, band_count, &bits_left);
                if (res < 0) return res;
            }
        }
    }

    // skip unused/reserved data (could use bm_skip but...)
    while (bits_left > 16) {
        read_bits(is, 16);
        bits_left -= 16;
    }
    if (bits_left > 0)
        read_bits(is, bits_left);

    return AT9_OK;
}


//-----------------------------------------------------------------------------
// UNPACK: BLOCKS

/* first frame_id in superframe must be 0, 1 otherwise (not in OG but added for clarity) */
static inline int check_frame_id(at9_core_t* core, int frame_id) {
    if (core->current_frame == 0 && frame_id != 0)
        return AT9_ERROR_UNPACK;
    if (core->current_frame > 0 && frame_id != 1)
        return AT9_ERROR_UNPACK;
    return AT9_OK;
}

/* Unpacks a mono/stereo block. channel_temps must hold 2 channels */
// OG: unpack_ab_at9
static int unpack_block_std(at9_core_t* core, bitstream_t* is, at9_block_temp_t* block_temp, at9_channel_temp_t* channel_temps, at9_block_t* block) {
    int res;

    // reuse band info from prev frame (can't be first frame in superframe)
    int reuse_band_info = read_bits(is, 1);
    if (reuse_band_info) {
        if (block_temp->frame_id == 0)
            return AT9_ERROR_UNPACK;
    }
    else {
        res = unpack_band_info(core, is, block);
        if (res < 0) return res;
    }

    res = unpack_gradient(is, block_temp, block);
    if (res < 0) return res;

    res = unpack_extra_params(is, block_temp, block);
    if (res < 0) return res;

    for (int ch = 0; ch < block->channel_count; ch++) {
        at9_channel_t* channel = block->channels[ch];
        at9_channel_temp_t* channel_temp = &channel_temps[ch]; //TODO: OG-like but ugly, maybe improve

        res = unpack_scalefactor_indexes(is, block_temp, block, channel, channel_temp, ch, block_temp->frame_id);
        if (res < 0) return res;

        calculate_word_lengths(block_temp, block, channel_temp);

        int qu_count = (block_temp->main_channel == ch) ? block->qu_count : block->stereo_qu_count;
        select_codebook_sets(channel_temp, qu_count);
        unpack_spectrum(core, is, channel_temp, qu_count);
        unpack_spectrum_fine(is, channel_temp, qu_count);
    }

    // intensity stereo: copy main channel's upper QUs to the other channel (L->R or R->L)
    if (block->type == AT9_BLOCK_STEREO) {
        const float* src_sp = channel_temps[block_temp->main_channel].spectrum;
        float* dst_sp = channel_temps[!block_temp->main_channel].spectrum;

        for (int qu = block->stereo_qu_count; qu < block->qu_count; qu++) {
            for (int i = qu_coef_start_table[qu]; i < qu_coef_start_table[qu + 1]; i++) {
                float coef = src_sp[i];
                if (block_temp->stereo_signs[qu])
                    coef = -coef;
                dst_sp[i] = coef;
            }
        }
    }

    //for (...) apply_scalefactors(...)

    block->qu_count_prev = block->bex_enabled ? block->bex_qu_count : block->qu_count;

    return res;
}

/* same but for mini blocks */
// OG: unpack_ab_at9_lfe
static int unpack_block_lfe(bitstream_t* is, at9_channel_temp_t* channel_temps, at9_block_t* block) {

    int band_info_absent = read_bits(is, 1); // 'reuse band info' flag in standard
    block->qu_count = 2;

    unpack_scalefactor_indexes_lfe(is, block, channel_temps);
    calculate_word_lengths_lfe(block, channel_temps, band_info_absent);
    unpack_spectrum_lfe(is, channel_temps, block->qu_count);
    //apply_scalefactors(...)

    return AT9_OK;
}

/* V3: simple mono block for haptics (vibration), that only need simple waves. */
static int unpack_block_vib(at9_core_t* core, bitstream_t* is, at9_channel_temp_t* channel_temps, at9_block_t* block) {
    at9_channel_temp_t* channel_temp = &channel_temps[0];

    read_bits(is, 1); // reuse band info flag? (seemingly ignored)
    int wl_mode = read_bits(is, 1);
    block->qu_count = read_bits(is, 4) + 1;

    // inline'd unpack_scalefactor_indexes_vib?
    memset(channel_temp->sf_indexes, 0, sizeof(channel_temp->sf_indexes));
    for (int qu = 0; qu < block->qu_count; qu++) {
        channel_temp->sf_indexes[qu] = read_bits(is, 5);
    }

    // inline'd calculate_word_lengths_vib?
    memset(channel_temp->wl_indexes, 0, sizeof(channel_temp->wl_indexes));
    memset(channel_temp->cl_indexes, 0, sizeof(channel_temp->cl_indexes));
    int wl_bits = wl_mode ? 2 : 3;
    for (int qu = 0; qu < block->qu_count; qu++) {
        channel_temp->wl_indexes[qu] = read_bits(is, wl_bits) + 1;
    }

    unpack_spectrum_vib(core, is, channel_temp, block->qu_count);

    return AT9_OK;
}

// OG: unpack_byte_alignment_at9
static void unpack_byte_alignment(bitstream_t* is) {
    int bit_pos = bm_pos(is);

    // TO-DO: maybe could use bm_align(is, 8)
    int bits = ((bit_pos + 7) & ~7) - bit_pos;
    if (bits > 0)
        read_bits(is, bits);
}


int unpack_block(at9_core_t* core, bitstream_t* is, at9_block_temp_t* block_temp, at9_block_t* block, at9_channel_temp_t* channel_temps) {
    int res;

    // OG does this on every block type, but there shouldn't be any difference (also passes current_frame to unpack_block_x)
    block_temp->frame_id = read_bits(is, 1);
    res = check_frame_id(core, block_temp->frame_id);
    if (res < 0)
        return res;

    if (block->type == AT9_BLOCK_LFE)
        res = unpack_block_lfe(is, channel_temps, block);
    else if (block->type == AT9_BLOCK_VIBRATION)
        res = unpack_block_vib(core, is, channel_temps, block);
    else
        res = unpack_block_std(core, is, block_temp, channel_temps, block);
    if (res < 0)
        return res;

    unpack_byte_alignment(is);

    return res;
}

// OG: unpack_frame_alignment_at9
int unpack_superframe_padding(bitstream_t* is, int bytes_left) {
    int padding = bytes_left - bm_pos(is) / 8;
    if (padding < 0)
        return AT9_ERROR_PADDING;

    // last frame in superframe is padded with 0x01
    for (int i = 0; i < padding; i++) {
        int padding_byte = read_bits(is, 8);
        if (padding_byte != 0x01)
            return AT9_ERROR_PADDING;
    }

    return AT9_OK;
}
