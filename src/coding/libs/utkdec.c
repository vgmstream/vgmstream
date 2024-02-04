#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "utkdec.h"

// AKA 'UTALKSTATE'
struct utk_context_t {
    /* config */ 
    utk_type_t type;
    int parsed_header;

    /* state */
    struct bitreader_t {
        const uint8_t* ptr;
        uint32_t bits_value;
        int bits_count;
        /* extra (OG MT/CBX just loads ptr memory externally) */
        const uint8_t* end;
        void* arg;
        uint8_t* buffer;
        size_t buffer_size;
        size_t (*read_callback)(void* dst, int size, void* arg);
    } br;
    bool reduced_bandwidth;
    int multipulse_threshold;

    float fixed_gains[64];
    float rc_data[12];
    float synth_history[12];
    float subframes[324 + 432];
    /* adapt_cb indexes may read from samples, join both + ptr to avoid
     * struct aligment issues (typically doesn't matter but for completeness) */
    float* adapt_cb; /* subframes + 0 */
    float* samples; /* subframes + 324 */
};


/* AKA 'bitmask'; (1 << count) - 1 is probably faster now but OG code uses a table */
static const uint8_t mask_table[8] = {
    0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF
};

/* AKA 'coeff_table', reflection coefficients (rounded) that correspond to hex values in exes (actual float is longer)
 * note this table is mirrored: for (i = 1 .. 32) t[64 - i] = -t[i]) */
static const float utk_rc_table[64] = {
    /* 6b index start */
    +0.000000f, -0.996776f, -0.990327f, -0.983879f,
    -0.977431f, -0.970982f, -0.964534f, -0.958085f,
    -0.951637f, -0.930754f, -0.904960f, -0.879167f,
    -0.853373f, -0.827579f, -0.801786f, -0.775992f,
    /* 5b index start */
    -0.750198f, -0.724405f, -0.698611f, -0.670635f,
    -0.619048f, -0.567460f, -0.515873f, -0.464286f,
    -0.412698f, -0.361111f, -0.309524f, -0.257937f,
    -0.206349f, -0.154762f, -0.103175f, -0.051587f,
    +0.000000f, +0.051587f, +0.103175f, +0.154762f,
    +0.206349f, +0.257937f, +0.309524f, +0.361111f,
    +0.412698f, +0.464286f, +0.515873f, +0.567460f,
    +0.619048f, +0.670635f, +0.698611f, +0.724405f,
    +0.750198f, +0.775992f, +0.801786f, +0.827579f,
    +0.853373f, +0.879167f, +0.904960f, +0.930754f,
    +0.951637f, +0.958085f, +0.964534f, +0.970982f,
    +0.977431f, +0.983879f, +0.990327f, +0.996776f,
};

// AKA 'index_table'
static const uint8_t utk_codebooks[2][256] = {
    /* normal model */
    {
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 25,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  0,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 21,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 26,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 17,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5, 22,
        4,  6,  5,  9,  4,  6,  5, 13,  4,  6,  5, 10,  4,  6,  5, 18,
        4,  6,  5,  9,  4,  6,  5, 14,  4,  6,  5, 10,  4,  6,  5,  2
    },
    /* large-pulse model */
    {
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 27,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  1,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 23,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8, 28,
        4, 11,  7, 15,  4, 12,  8, 19,  4, 11,  7, 16,  4, 12,  8, 24,
        4, 11,  7, 15,  4, 12,  8, 20,  4, 11,  7, 16,  4, 12,  8,  3
    },
};

enum {
    MDL_NORMAL = 0,
    MDL_LARGEPULSE = 1
};

// AKA 'decode_table'
static const struct {
    int next_model;
    int code_size;
    float pulse_value;
} utk_commands[29] = {
    {MDL_LARGEPULSE, 8,  0.0f},
    {MDL_LARGEPULSE, 7,  0.0f},
    {MDL_NORMAL,     8,  0.0f},
    {MDL_NORMAL,     7,  0.0f},
    {MDL_NORMAL,     2,  0.0f},
    {MDL_NORMAL,     2, -1.0f},
    {MDL_NORMAL,     2, +1.0f},
    {MDL_NORMAL,     3, -1.0f},
    {MDL_NORMAL,     3, +1.0f},
    {MDL_LARGEPULSE, 4, -2.0f},
    {MDL_LARGEPULSE, 4, +2.0f},
    {MDL_LARGEPULSE, 3, -2.0f},
    {MDL_LARGEPULSE, 3, +2.0f},
    {MDL_LARGEPULSE, 5, -3.0f},
    {MDL_LARGEPULSE, 5, +3.0f},
    {MDL_LARGEPULSE, 4, -3.0f},
    {MDL_LARGEPULSE, 4, +3.0f},
    {MDL_LARGEPULSE, 6, -4.0f},
    {MDL_LARGEPULSE, 6, +4.0f},
    {MDL_LARGEPULSE, 5, -4.0f},
    {MDL_LARGEPULSE, 5, +4.0f},
    {MDL_LARGEPULSE, 7, -5.0f},
    {MDL_LARGEPULSE, 7, +5.0f},
    {MDL_LARGEPULSE, 6, -5.0f},
    {MDL_LARGEPULSE, 6, +5.0f},
    {MDL_LARGEPULSE, 8, -6.0f},
    {MDL_LARGEPULSE, 8, +6.0f},
    {MDL_LARGEPULSE, 7, -6.0f},
    {MDL_LARGEPULSE, 7, +6.0f}
};

/* In Lego Batman 2 gain[0] = 1.068 while other games (Lego Marvel, Lego SW) is 64.0f.
 * The latter makes more sense and the former is audibly worse so it was probably a bug. */
static const float cbx_fixed_gains[64] = {
    64.0f,      68.351997f, 72.999931f, 77.963921f,
    83.265465f, 88.927513f, 94.974579f, 101.43285f,
    108.33028f, 115.69673f, 123.5641f,  131.96646f,
    140.94017f, 150.52409f, 160.75972f, 171.69138f,
    183.36638f, 195.83528f, 209.15207f, 223.3744f,
    238.56386f, 254.78619f, 272.11163f, 290.6152f,
    310.37701f, 331.48264f, 354.02344f, 378.09702f,
    403.80759f, 431.26648f, 460.59259f, 491.91287f,
    525.36292f, 561.08759f, 599.24152f, 639.98993f,
    683.50922f, 729.98779f, 779.62695f, 832.64154f,
    889.26111f, 949.73083f, 1014.3125f, 1083.2858f,
    1156.9491f, 1235.6216f, 1319.6438f, 1409.3795f,
    1505.2173f, 1607.572f,  1716.8868f, 1833.6351f,
    1958.3223f, 2091.488f,  2233.7092f, 2385.6013f,
    2547.822f,  2721.0737f, 2906.1067f, 3103.7219f,
    3314.7749f, 3540.1794f, 3780.9116f, 4038.0134f,
};

/* Bitreader in OG code can only read from set ptr; doesn't seem to check bounds though.
 * Incidentally bitreader functions seem to be used only in MT and not in other EA stuff. */
static uint8_t read_byte(struct bitreader_t* br) {
    if (br->ptr < br->end)
        return *br->ptr++;

    if (br->read_callback) {
        size_t bytes_copied = br->read_callback(br->buffer, br->buffer_size, br->arg);
        if (bytes_copied > 0 && bytes_copied <= br->buffer_size) {
            br->ptr = br->buffer;
            br->end = br->buffer + bytes_copied;
            return *br->ptr++;
        }
    }

    return 0;
}

static int16_t read_s16(struct bitreader_t* br) {
    int x = read_byte(br);
    x = (x << 8) | read_byte(br);
    return x;
}

static void init_bits(struct bitreader_t* br) {
    if (!br->bits_count) {
        br->bits_value = read_byte(br);
        br->bits_count = 8;
    }
}

static uint8_t peek_bits(struct bitreader_t* br, int count) {
    uint8_t mask = mask_table[count - 1];
    return br->bits_value & mask;
}

/* aka 'getbits', LSB style and assumes count <= 8, which is always true since sizes are known and don't depend on the bitstream. */
static uint8_t read_bits(struct bitreader_t* br, int count) {
    uint8_t mask = mask_table[count - 1];
    uint8_t ret = br->bits_value & mask;
    br->bits_value >>= count;
    br->bits_count -= count;

    if (br->bits_count < 8) {
        /* read another byte */
        br->bits_value |= read_byte(br) << br->bits_count;
        br->bits_count += 8;
    }

    return ret;
}

/* AKA 'discardbits', as found in OG code (no return) */
static void consume_bits(struct bitreader_t* br, int count) {
    read_bits(br, count);
}

static void parse_header(utk_context_t* ctx) {
    if (ctx->type == UTK_CBX) {
        /* CBX uses fixed parameters unlike EA-MT, probably encoder defaults for MT10:1
         * equivalent to EA-MT with base_thre = 8, base_gain = 7, base_mult = 28 (plus rounding diffs).
         * OG CBX code uses values/tables directly rather than config though */
        ctx->reduced_bandwidth = true;

        ctx->multipulse_threshold = 32 - 8;

        ctx->fixed_gains[0] = cbx_fixed_gains[0];
        for (int i = 1; i < 64; i++) {
            ctx->fixed_gains[i] = cbx_fixed_gains[i];
        }
    }
    else {
        ctx->reduced_bandwidth = read_bits(&ctx->br, 1) == 1;

        int base_thre = read_bits(&ctx->br, 4);
        int base_gain = read_bits(&ctx->br, 4);
        int base_mult = read_bits(&ctx->br, 6);

        ctx->multipulse_threshold = 32 - base_thre;
        ctx->fixed_gains[0] = 8.0f * (1 + base_gain);

        float multiplier = 1.04f + base_mult * 0.001f;
        for (int i = 1; i < 64; i++) {
            ctx->fixed_gains[i] = ctx->fixed_gains[i-1] * multiplier;
        }
    }
}

static void decode_excitation(utk_context_t* ctx, bool use_multipulse, float* out, int stride) {
    int i = 0;

    if (use_multipulse) {
        /* multi-pulse model: n pulses are coded explicitly; the rest are zero */
        int model = 0;
        while (i < 108) {
            int huffman_code = peek_bits(&ctx->br, 8); /* variable-length, may consume less */

            int cmd = utk_codebooks[model][huffman_code];
            model = utk_commands[cmd].next_model;

            consume_bits(&ctx->br, utk_commands[cmd].code_size);

            if (cmd > 3) {
                /* insert a pulse with magnitude <= 6.0f */
                out[i] = utk_commands[cmd].pulse_value;
                i += stride;
            }
            else if (cmd > 1) {
                /* insert between 7 and 70 zeros */
                int count = 7 + read_bits(&ctx->br, 6);
                if (i + count * stride > 108)
                    count = (108 - i) / stride;

                while (count > 0) {
                    out[i] = 0.0f;
                    i += stride;
                    count--;
                }
            }
            else {
                /* insert a pulse with magnitude >= 7.0f */
                int x = 7;

                while (read_bits(&ctx->br, 1)) {
                    x++;
                }

                if (!read_bits(&ctx->br, 1))
                    x *= -1;

                out[i] = (float)x;
                i += stride;
            }
        }
    }
    else {
        /* RELP model: entire residual (excitation) signal is coded explicitly */
        while (i < 108) {
            int bits = 0;
            float val = 0.0f;

            /* peek + partial consume code (bitreader is LSB so this is equivalent to reading bit by bit, but OG handles it like this) */
            int huffman_code = peek_bits(&ctx->br, 2); /* variable-length, may consume less */
            switch (huffman_code) {
                case 0: //value 00 = h.code: 0
                case 2: //value 10 = h.code: 0
                    val = 0.0f;
                    bits = 1;
                    break;
                case 1: //value 01 = h.code: 10
                    val = -2.0f;
                    bits = 2;
                    break;
                case 3: //value 11 = h.code: 11
                    val = 2.0f;
                    bits = 2;
                    break;
                default:
                    break;
            }
            consume_bits(&ctx->br, bits);

            out[i] = val;
            i += stride;
        }
    }
}

// AKA ref_to_lpc
static void rc_to_lpc(const float* rc_data, float* lpc) {
    int j;
    float tmp1[12];
    float tmp2[12];

    for (int i = 10; i >= 0; i--) {
        tmp2[i + 1] = rc_data[i];
    }

    tmp2[0] = 1.0f;

    for (int i = 0; i < 12; i++) {
        float x = -(rc_data[11] * tmp2[11]);

        for (j = 10; j >= 0; j--) {
            x -= (rc_data[j] * tmp2[j]);
            tmp2[j + 1] = x * rc_data[j] + tmp2[j];
        }

        tmp2[0] = x;
        tmp1[i] = x;

        for (j = 0; j < i; j++) {
            x -= tmp1[i - 1 - j] * lpc[j];
        }

        lpc[i] = x;
    }
}

// AKA 'filter'
static void lp_synthesis_filter(utk_context_t* ctx, int offset, int blocks) {
    int i, j, k;
    float lpc[12];
    float* ptr = &ctx->samples[offset];

    rc_to_lpc(ctx->rc_data, lpc);

    for (i = 0; i < blocks; i++) {
        /* OG: unrolled x12*12 */
        for (j = 0; j < 12; j++) {
            float x = *ptr;

            for (k = 0; k < j; k++) {
                x += lpc[k] * ctx->synth_history[k - j + 12];
            }
            for (; k < 12; k++) {
                x += lpc[k] * ctx->synth_history[k - j + 0];
            }

            ctx->synth_history[11 - j] = x;

            *ptr++ = x;

            /* CBX only: samples are multiplied by 12582912.0, then coerce_int(sample[i]) on output
             * to get final int16, as a pseudo-optimization; not sure if worth replicating */
        }
    }
}

// AKA 'interpolate', OG sometimes inlines this (sx3, not B&B/CBX) */
static void interpolate_rest(float* excitation) {
    for (int i = 0; i < 108; i += 2) {
        float tmp1 = (excitation[i - 5] + excitation[i + 5]) * 0.01803268f;
        float tmp2 = (excitation[i - 3] + excitation[i + 3]) * 0.11459156f;
        float tmp3 = (excitation[i - 1] + excitation[i + 1]) * 0.59738597f;
        excitation[i] = tmp1 - tmp2 + tmp3;
    }
}

// AKA 'decodemut'
static void decode_frame_main(utk_context_t* ctx) {
    bool use_multipulse = false;
    float excitation[5 + 108 + 5]; /* extra +5*2 for interpolation */
    float rc_delta[12];

    /* OG code usually calls this init/parse header after creation rather than on frame decode,
     * but use a flag for now since buffer can be set/reset after init */
    init_bits(&ctx->br);

    if (!ctx->parsed_header) {
        parse_header(ctx);
        ctx->parsed_header = 1;
    }


    /* read the reflection coefficients (OG unrolled) */
    for (int i = 0; i < 12; i++) {
        int idx;
        if (i == 0) {
            idx = read_bits(&ctx->br, 6);
            if (idx < ctx->multipulse_threshold)
                use_multipulse = true;
        }
        else if (i < 4) {
            idx = read_bits(&ctx->br, 6);
        }
        else {
            idx = 16 + read_bits(&ctx->br, 5);
        }

        rc_delta[i] = (utk_rc_table[idx] - ctx->rc_data[i]) * 0.25f;
    }


    /* decode four subframes (AKA 'readsamples' but inline'd) */
    for (int i = 0; i < 4; i++) {
        int pitch_lag = read_bits(&ctx->br, 8);
        int pitch_value = read_bits(&ctx->br, 4);
        int gain_index = read_bits(&ctx->br, 6);

        float pitch_gain = (float)pitch_value / 15.0f; /* may be compiled as: value * 0.6..67 (float or double) */
        float fixed_gain = ctx->fixed_gains[gain_index];

        if (!ctx->reduced_bandwidth) {
            /* full bandwidth (probably MT5:1) */
            decode_excitation(ctx, use_multipulse, &excitation[5 + 0], 1);
            /* OG: CBX doesn't have this flag and removes the if (so not 100% same code as MT) */
        }
        else {
            /* residual (excitation) signal is encoded at reduced bandwidth */
            int align = read_bits(&ctx->br, 1);
            int zero_flag = read_bits(&ctx->br, 1);

            decode_excitation(ctx, use_multipulse, &excitation[5 + align], 2);

            if (zero_flag) {
                /* fill the remaining samples with zero (spectrum is duplicated into high frequencies) */
                for (int j = 0; j < 54; j++) {
                    excitation[5 + (1 - align) + 2 * j] = 0.0f;
                }
            }
            else {
                /* 0'd first + last samples for interpolation */
                memset(&excitation[0], 0, 5 * sizeof(float));
                memset(&excitation[5 + 108], 0, 5 * sizeof(float));
                
                /* interpolate the remaining samples (spectrum is low-pass filtered) */
                interpolate_rest(&excitation[5 + (1 - align)]);

                /* scale by 0.5f to give the sinc impulse response unit energy */
                fixed_gain *= 0.5f;
            }
        }

        /* OG: sometimes unrolled */
        for (int j = 0; j < 108; j++) {
            /* This has potential to read garbage from fixed_gains/samples (-39 ~ +648). The former
             * seems avoided by the encoder but we'll clamp it just in case, while the later is common
             * and seemingly used on purpose, so it's allowed via joining adapt_cb + samples bufs. */
            int idx = 108 * i + 216 - pitch_lag + j;
            if (idx < 0) /* OG: not done but shouldn't matter */
                idx = 0;

            float tmp1 = fixed_gain * excitation[5 + j];
            float tmp2 = pitch_gain * ctx->adapt_cb[idx];
            ctx->samples[108 * i + j] = tmp1 + tmp2;
        }
    }

    /* OG: may be compiler-optimized to memcpy */
    for (int i = 0; i < 324; i++) {
        ctx->adapt_cb[i] = ctx->samples[108 + i];
    }

    /* OG: unrolled x4 */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 12; j++) {
            ctx->rc_data[j] += rc_delta[j];
        }

        int blocks = i < 3 ? 1 : 33;
        lp_synthesis_filter(ctx, 12 * i, blocks);
    }
}

static int decode_frame_pcm(utk_context_t* ctx) {
    int pcm_data_present = (read_byte(&ctx->br) == 0xEE);
    int i;

    decode_frame_main(ctx);

    /* unread the last 8 bits and reset the bit reader 
     * (a bit odd but should be safe in all cases, assuming ptr has been set) */
    ctx->br.ptr--;
    ctx->br.bits_count = 0;

    if (pcm_data_present) {
        /* Overwrite n samples at a given offset in the decoded frame with raw PCM data. */
        int offset = read_s16(&ctx->br);
        int count = read_s16(&ctx->br);

        /* sx.exe does not do any bounds checking or clamping of these two
         * fields (see 004274D1 in sx.exe v3.01.01), which means a specially
         * crafted MT5:1 file can crash it. We will throw an error instead. */
        if (offset < 0 || offset > 432) {
            return -1; /* invalid PCM offset */
        }
        if (count < 0 || count > 432 - offset) {
            return -2; /* invalid PCM count */
        }

        for (i = 0; i < count; i++) {
            ctx->samples[offset+i] = (float)read_s16(&ctx->br);
        }
    }

    return 432;
}

//

int utk_decode_frame(utk_context_t* ctx) {
    if (ctx->type == UTK_EA_PCM) {
        return decode_frame_pcm(ctx);
    }
    else {
        decode_frame_main(ctx);
        return 432;
    }
}

utk_context_t* utk_init(utk_type_t type) {
    utk_context_t* ctx = calloc(1, sizeof(utk_context_t));
    if (!ctx) return NULL;
    
    //memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    
    ctx->adapt_cb = ctx->subframes + 0;
    ctx->samples = ctx->subframes + 324;
    
    return ctx;
}

void utk_free(utk_context_t* ctx) {
    free(ctx);    
}

void utk_reset(utk_context_t* ctx) {
    /* resets the internal state, leaving the external config/buffers
     * untouched (could be reset externally or using utk_set_x) */
    ctx->parsed_header = 0;
    ctx->br.bits_value = 0;
    ctx->br.bits_count = 0;
    ctx->reduced_bandwidth = 0;
    ctx->multipulse_threshold = 0;
    memset(ctx->fixed_gains, 0, sizeof(ctx->fixed_gains));
    memset(ctx->rc_data, 0, sizeof(ctx->rc_data));
    memset(ctx->synth_history, 0, sizeof(ctx->synth_history));
    memset(ctx->subframes, 0, sizeof(ctx->subframes));
}

void utk_set_callback(utk_context_t* ctx, uint8_t* buffer, size_t buffer_size, void *arg, size_t (*read_callback)(void *, int , void *)) {
    ctx->br.buffer = buffer;
    ctx->br.buffer_size = buffer_size;
    ctx->br.arg = arg;
    ctx->br.read_callback = read_callback;

    /* reset the bit reader */
    ctx->br.bits_count = 0;
}

void utk_set_buffer(utk_context_t* ctx, const uint8_t* buf, size_t buf_size) {
    ctx->br.ptr = buf;
    ctx->br.end = buf + buf_size;

    /* reset the bit reader */
    ctx->br.bits_count = 0;
}

float* utk_get_samples(utk_context_t* ctx) {
    return ctx->samples;
}
