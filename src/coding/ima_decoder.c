#include "../util.h"
#include "coding.h"

/**
 * IMA ADPCM algorithms (expand one nibble to one sample, based on prev sample/history and step table).
 * Nibbles are usually grouped in blocks/chunks, with a header, containing 1 or N channels
 *
 * All IMAs are mostly the same with these variations:
 * - interleave: blocks and channels are handled externally (layouts) or internally (mixed channels)
 * - block header: none (external), normal (4 bytes of history 16b + step 8b + reserved 8b) or others; per channel/global
 * - expand type: IMA style or variations; low or high nibble first
 */

static const int ADPCMTable[90] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,

    0 /* garbage value for Ubisoft IMA (see blocked_ubi_sce.c) */
};

static const int IMA_IndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8 
};


/* Original IMA expansion, using shift+ADDs to avoid MULs (slow back then) */
static void std_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    /* simplified through math from:
     *  - diff = (code + 1/2) * (step / 4)
     *   > diff = ((step * nibble) + (step / 2)) / 4
     *    > diff = (step * nibble / 4) + (step / 8)
     * final diff = [signed] (step / 8) + (step / 4) + (step / 2) + (step) [when code = 4+2+1] */

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf; /* ADPCM code */
    sample_decoded = *hist1; /* predictor value */
    step = ADPCMTable[*step_index]; /* current step */

    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Apple's IMA variation. Exactly the same except it uses 16b history (probably more sensitive to overflow/sign extend?) */
static void std_ima_expand_nibble_16(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int16_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded); /* no need for this, actually */
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Original IMA expansion, but using MULs rather than shift+ADDs (faster for newer processors).
 * There is minor rounding difference between ADD and MUL expansions, noticeable/propagated in non-headered IMAs. */
static void std_ima_expand_nibble_mul(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    /* simplified through math from:
     *  - diff = (code + 1/2) * (step / 4)
     *   > diff = (code + 1/2) * step) / 4) * (2 / 2)
     *    > diff = (code + 1/2) * 2 * step / 8
     * final diff = [signed] ((code * 2 + 1) * step) / 8 */

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = (sample_nibble & 0x7);
    delta = ((delta * 2 + 1) * step) >> 3;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* NintendoWare IMA (Mario Golf, Mario Tennis; maybe other Camelot games) */
static void nw_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    sample_decoded = sample_decoded << 3;
    delta = (sample_nibble & 0x07);
    delta = step * delta * 2 + step; /* custom */
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;
    sample_decoded = sample_decoded >> 3;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* The Incredibles PC, updates step_index before doing current sample */
static void snds_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;

    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;

    step = ADPCMTable[*step_index];

    delta = (sample_nibble & 7) * step / 4 + step / 8; /* standard IMA */
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
}

/* Omikron: The Nomad Soul, algorithm from the .exe */
static void otns_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = 0;
    if(sample_nibble & 4) delta = step * 4;
    if(sample_nibble & 2) delta += step * 2;
    if(sample_nibble & 1) delta += step;
    delta >>= 2;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Fairly OddParents (PC) .WV6: minor variation, reverse engineered from the .exe */
static void wv6_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = (sample_nibble & 0x7);
    delta = ((delta * step) >> 3) + ((delta * step) >> 2);
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* High Voltage variation, reverse engineered from .exes [Lego Racers (PC), NBA Hangtime (PC)] */
static void hv_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = (sample_nibble & 0x7);
    delta = (delta * step) >> 2;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    *hist1 = clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* FFTA2 IMA, different hist and sample rounding, reverse engineered from the ROM */
static void ffta2_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index, int16_t *out_sample) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf; /* ADPCM code */
    sample_decoded = *hist1; /* predictor value */
    step = ADPCMTable[*step_index] * 0x100; /* current step (table in ROM is pre-multiplied though) */

    delta = step >> 3;
    if (sample_nibble & 1) delta += step >> 2;
    if (sample_nibble & 2) delta += step >> 1;
    if (sample_nibble & 4) delta += step;
    if (sample_nibble & 8) delta = -delta;
    sample_decoded += delta;

    /* custom clamp16 */
    if (sample_decoded > 0x7FFF00)
        sample_decoded = 0x7FFF00;
    else if (sample_decoded < -0x800000)
        sample_decoded = -0x800000;

    *hist1 = sample_decoded;
    *out_sample = (short)((sample_decoded + 128) / 256); /* int16 sample rounding, hist is kept as int32 */

    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* Yet another IMA expansion, from the exe */
static void blitz_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf; /* ADPCM code */
    sample_decoded = *hist1; /* predictor value */
    step = ADPCMTable[*step_index]; /* current step */

    /* table has 2 different values, not enough to bother adding the full table */
    if (step == 22385)
        step = 22358;
    else if (step == 24623)
        step = 24633;

    delta = (sample_nibble & 0x07);
    if (sample_nibble & 8) delta = -delta;
    delta = (step >> 1) + delta * step; /* custom */
    sample_decoded += delta;

    /* in Zapper somehow the exe tries to clamp hist but actually doesn't (bug? not in Lilo & Stitch),
     * seems the pcm buffer must be clamped outside though to fix some scratchiness */
    *hist1 = sample_decoded;//clamp16(sample_decoded);
    *step_index += IMA_IndexTable[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

static const int CIMAADPCM_INDEX_TABLE[16] = {8,  6,  4,  2,  -1, -1, -1, -1,
                                             -1, -1, -1, -1, 2,  4,  6,  8};

/* Capcom's MT Framework modified IMA, reverse engineered from the exe */
static void mtf_ima_expand_nibble(VGMSTREAMCHANNEL * stream, off_t byte_offset, int nibble_shift, int32_t * hist1, int32_t * step_index) {
    int sample_nibble, sample_decoded, step, delta;

    sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift) & 0xf;
    sample_decoded = *hist1;
    step = ADPCMTable[*step_index];

    delta = step * (2 * sample_nibble - 15);
    sample_decoded += delta;

    *hist1 = sample_decoded;
    *step_index += CIMAADPCM_INDEX_TABLE[sample_nibble];
    if (*step_index < 0) *step_index=0;
    if (*step_index > 88) *step_index=88;
}

/* IMA table pre-modified like this:
     for i=0..89
       adpcm = clamp(adpcm[i], 0x1fff) * 4; 
*/
static const int16_t mul_adpcm_table[89] = {
    28,    32,    36,    40,    44,    48,    52,    56,
    64,    68,    76,    84,    92,    100,   112,   124,
    136,   148,   164,   180,   200,   220,   240,   264,
    292,   320,   352,   388,   428,   472,   520,   572,
    628,   692,   760,   836,   920,   1012,  1116,  1228,
    1348,  1484,  1632,  1796,  1976,  2176,  2392,  2632,
    2896,  3184,  3504,  3852,  4240,  4664,  5128,  5644,
    6208,  6828,  7512,  8264,  9088,  9996,  10996, 12096,
    13308, 14640, 16104, 17712, 19484, 21432, 23576, 25936,
    28528, 31380, 32764, 32764, 32764, 32764, 32764, 32764,
    32764, 32764, 32764, 32764, 32764, 32764, 32764, 32764,
    32764
};

/* step table is the same */

/* ops per code, generated like this:
    for i=0..15
        v = 0x800
        if (i & 1) v  = 0x1800
        if (i & 2) v += 0x2000
        if (i & 4) v += 0x4000
        if (i & 8) v = -v;
        mul_op_table[i] = v;
*/
static const int16_t mul_delta_table[16] = {
    0x0800, 0x1800, 0x2800, 0x3800, 0x4800, 0x5800, 0x6800, 0x7800,
   -0x0800,-0x1800,-0x2800,-0x3800,-0x4800,-0x5800,-0x6800,-0x7800
};


/* Crystal Dynamics IMA, reverse engineered from the exe, also info: https://github.com/sephiroth99/MulDeMu */
static void cd_ima_expand_nibble(uint8_t byte, int shift, int32_t* hist1, int32_t* index) {
    int code, sample, step, delta;

    /* could do the above table calcs during decode too */
    code = (byte >> shift) & 0xf;
    sample = *hist1;
    step = mul_adpcm_table[*index];

    delta = (int16_t)((step * mul_delta_table[code]) >> 16);
    sample += delta;

    *hist1 = clamp16(sample);
    *index += IMA_IndexTable[code];
    if (*index < 0) *index=0;
    if (*index > 88) *index=88;
}

/* ************************************ */
/* DVI/IMA                              */
/* ************************************ */

/* Standard DVI/IMA ADPCM (as in, ADPCM recommended by the IMA using Intel/DVI's implementation).
 * Configurable: stereo or mono/interleave nibbles, and high or low nibble first.
 * For vgmstream, low nibble is called "IMA ADPCM" and high nibble is "DVI IMA ADPCM" (same thing though). */
void decode_standard_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first) {
    int i, sample_count = 0;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave */

    /* no header (external setup), pre-clamp for wrong values */
    if (step_index < 0) step_index=0;
    if (step_index > 88) step_index=88;

    /* decode nibbles (layout: varies) */
    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_mtf_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo) {
    int i, sample_count = 0;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave */

    /* no header (external setup), pre-clamp for wrong values */
    if (step_index < 0) step_index=0;
    if (step_index > 88) step_index=88;

    /* decode nibbles (layout: varies) */
    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int nibble_shift = is_stereo ?
                ((channel&1) ? 0:4) :
                ((i&1) ? 0:4);

        mtf_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = clamp16(hist1 >> 4);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_nw_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?4:0); //low nibble order

        nw_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_snds_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i;//one nibble per channel
        int nibble_shift = (channel==0?0:4); //high nibble first, based on channel

        snds_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_otns_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal/byte interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + (vgmstream->channels==1 ? i/2 : i); //one nibble per channel if stereo
        int nibble_shift = (vgmstream->channels==1) ? //todo simplify
                    (i&1?0:4) : //high nibble first(?)
                    (channel==0?4:0); //low=ch0, high=ch1 (this is correct compared to vids)

        otns_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* WV6 IMA, DVI IMA with custom nibble expand */
void decode_wv6_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        wv6_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* High Voltage's DVI IMA with simplified nibble expand */
void decode_hv_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        hv_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* FFTA2 IMA, DVI IMA with custom nibble expand/rounding */
void decode_ffta2_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;
    int16_t out_sample;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        ffta2_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index, &out_sample);
        outbuf[sample_count] = out_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* Blitz IMA, IMA with custom nibble expand */
void decode_blitz_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //external interleave

    //no header

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        blitz_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)clamp16(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* ************************************ */
/* MS-IMA                               */
/* ************************************ */

/* IMA with custom frame sizes, header and nibble layout. Outputs an odd number of samples per frame,
 * so to simplify calcs this decodes full frames, thus hist doesn't need to be mantained.
 * Officially defined in "Microsoft Multimedia Standards Update" doc (RIFFNEW.pdf). */
void decode_ms_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, samples_read = 0, samples_done = 0, max_samples;
    int32_t hist1;// = stream->adpcm_history1_32;
    int step_index;// = stream->adpcm_step_index;
    int frame_channels = vgmstream->codec_config ? 1 : vgmstream->channels; /* mono or mch modes */
    int frame_channel =  vgmstream->codec_config ? 0 : channel;

    /* internal interleave (configurable size), mixed channels */
    int block_samples = ((vgmstream->frame_size - 0x04*frame_channels) * 2 / frame_channels) + 1;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved), per channel */
    { //if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x04*frame_channel;

        hist1 =   read_s16le(header_offset+0x00,stream->streamfile);
        step_index = read_u8(header_offset+0x02,stream->streamfile); /* 0x03: reserved */
        if (step_index < 0) step_index = 0;
        if (step_index > 88) step_index = 88;

        /* write header sample (odd samples per block) */
        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)hist1;
            samples_done++;
        }
        samples_read++;
    }

    max_samples = (block_samples - samples_read);
    if (max_samples > samples_to_do + first_sample - samples_done)
        max_samples = samples_to_do + first_sample - samples_done; /* for smaller last block */

    /* decode nibbles (layout: alternates 4 bytes/4*2 nibbles per channel) */
    for (i = 0; i < max_samples; i++) {
        off_t byte_offset = stream->offset + 0x04*frame_channels + 0x04*frame_channel + 0x04*frame_channels*(i/8) + (i%8)/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index); /* original expand */

        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)(hist1);
            samples_done++;
        }
        samples_read++;
    }

    /* internal interleave: increment offset on complete frame */
    if (first_sample + samples_done == block_samples)  {
        stream->offset += vgmstream->frame_size;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_step_index = step_index;
}

/* Reflection's MS-IMA with custom nibble layout (some info from XA2WAV by Deniz Oezmen) */
void decode_ref_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, samples_read = 0, samples_done = 0, max_samples;
    int32_t hist1;// = stream->adpcm_history1_32;
    int step_index;// = stream->adpcm_step_index;

    /* internal interleave (configurable size), mixed channels */
    int block_channel_size = (vgmstream->interleave_block_size - 0x04*vgmstream->channels) / vgmstream->channels;
    int block_samples = ((vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved), per channel */
    { //if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x04*channel;

        hist1 =   read_16bitLE(header_offset+0x00,stream->streamfile);
        step_index = read_8bit(header_offset+0x02,stream->streamfile);
        if (step_index < 0) step_index = 0;
        if (step_index > 88) step_index = 88;

        /* write header sample */
        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)hist1;
            samples_done++;
        }
        samples_read++;
    }

    max_samples = (block_samples - samples_read);
    if (max_samples > samples_to_do + first_sample - samples_done)
        max_samples = samples_to_do + first_sample - samples_done; /* for smaller last block */

    /* decode nibbles (layout: all nibbles from one channel, then other channels) */
    for (i = 0; i < max_samples; i++) {
        off_t byte_offset = stream->offset + 0x04*vgmstream->channels + block_channel_size*channel + i/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);

        if (samples_read >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (short)(hist1);
            samples_done++;
        }
        samples_read++;
    }

    /* internal interleave: increment offset on complete frame */
    if (first_sample + samples_done == block_samples)  {
        stream->offset += vgmstream->interleave_block_size;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_step_index = step_index;
}

/* ************************************ */
/* XBOX-IMA                             */
/* ************************************ */

/* MS-IMA with fixed frame size, and outputs an even number of samples per frame (skips last nibble).
 * Defined in Xbox's SDK. Usable in mono or stereo modes (both suitable for interleaved multichannel). */
void decode_xbox_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo) {
    int i, frames_in, sample_pos = 0, block_samples, frame_size;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;
    off_t frame_offset;

    /* external interleave (fixed size), stereo/mono */
    block_samples = (0x24 - 0x4) * 2;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;
    frame_size = is_stereo ? 0x24*2 : 0x24;

    frame_offset = stream->offset + frame_size*frames_in;

    /* normal header (hist+step+reserved), stereo/mono */
    if (first_sample == 0) {
        off_t header_offset = is_stereo ?
                frame_offset + 0x04*(channel % 2) :
                frame_offset + 0x00;

        hist1   = read_16bitLE(header_offset+0x00,stream->streamfile);
        step_index = read_8bit(header_offset+0x02,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;

        /* write header sample (even samples per block, skips last nibble) */
        outbuf[sample_pos] = (short)(hist1);
        sample_pos += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    /* decode nibbles (layout: straight in mono or 4 bytes per channel in stereo) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        off_t byte_offset = is_stereo ?
                frame_offset + 0x04*2 + 0x04*(channel % 2) + 0x04*2*((i-1)/8) + ((i-1)%8)/2 :
                frame_offset + 0x04   + (i-1)/2;
        int nibble_shift = (!((i-1)&1)   ? 0:4);   /* low first */

        /* must skip last nibble per spec, rarely needed though (ex. Gauntlet Dark Legacy) */
        if (i < block_samples) {
            std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_pos] = (short)(hist1);
            sample_pos += channelspacing;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* Multichannel XBOX-IMA ADPCM, with all channels mixed in the same block (equivalent to multichannel MS-IMA; seen in .rsd XADP). */
void decode_xbox_ima_mch(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0, num_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave (fixed size), multichannel */
    int block_samples = (0x24 - 0x4) * 2;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved), multichannel */
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x24*channelspacing*num_frame + 0x04*channel;

        hist1   = read_16bitLE(header_offset+0x00,stream->streamfile);
        step_index = read_8bit(header_offset+0x02,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;

        /* write header sample (even samples per block, skips last nibble) */
        outbuf[sample_count] = (short)(hist1);
        sample_count += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    /* decode nibbles (layout: alternates 4 bytes/4*2 nibbles per channel) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        off_t byte_offset = (stream->offset + 0x24*channelspacing*num_frame + 0x04*channelspacing) + 0x04*channel + 0x04*channelspacing*((i-1)/8) + ((i-1)%8)/2;
        int nibble_shift = ((i-1)&1?4:0); /* low nibble first */

        /* must skip last nibble per spec, rarely needed though */
        if (i < block_samples) {
            std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count += channelspacing;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* Similar to MS-IMA with even number of samples, header sample is not written (setup only).
 * Apparently clamps to -32767 unlike standard's -32768 (probably not noticeable).
 * Info here: http://problemkaputt.de/gbatek.htm#dssoundnotes */
void decode_nds_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave (configurable size), mono */

    /* normal header (hist+step+reserved), single channel */
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0; /* probably pre-adjusted */
        if (step_index > 88) step_index=88;
    }

    /* decode nibbles (layout: all nibbles from the channel) */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 0x04 + i/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        //todo waveform has minor deviations using known expands
        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_dat4_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    //external interleave

    //normal header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        hist1 = read_16bitLE(header_offset,stream->streamfile);
        step_index = read_8bit(header_offset+2,stream->streamfile);

        //todo clip step_index?
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?0:4); //high nibble first

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_rad_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave (configurable size), mixed channels (4 byte per ch)
    int block_samples = (vgmstream->interleave_block_size - 4*vgmstream->channels) * 2 / vgmstream->channels;
    first_sample = first_sample % block_samples;

    //inverted header (per channel)
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 4*channel;

        step_index = read_16bitLE(header_offset,stream->streamfile);
        hist1 = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4*vgmstream->channels + channel + i/2*vgmstream->channels;
        int nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += vgmstream->interleave_block_size;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

void decode_rad_ima_mono(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //semi-external interleave?
    int block_samples = 0x14 * 2;
    first_sample = first_sample % block_samples;

    //inverted header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        step_index = read_16bitLE(header_offset,stream->streamfile);
        hist1 = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* Apple's IMA4, a.k.a QuickTime IMA. 2 byte header and header sample is not written (setup only). */
void decode_apple_ima4(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count, num_frame;
    int16_t hist1 = stream->adpcm_history1_16;//todo unneeded 16?
    int step_index = stream->adpcm_step_index;

    //external interleave
    int block_samples = (0x22 - 0x2) * 2;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    //2-byte header
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x22*num_frame;

        hist1 = (int16_t)((uint16_t)read_16bitBE(header_offset,stream->streamfile) & 0xff80);
        step_index = read_8bit(header_offset+1,stream->streamfile) & 0x7f;
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = (stream->offset + 0x22*num_frame + 0x2) + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble_16(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_index;
}

/* XBOX-IMA with modified data layout */
void decode_fsb_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    int i, sample_count = 0;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* internal interleave (configurable size), mixed channels */
    int block_samples = (0x24 - 0x4) * 2;
    first_sample = first_sample % block_samples;

    /* interleaved header (all hist per channel + all step_index+reserved per channel) */
    if (first_sample == 0) {
        off_t hist_offset = stream->offset + 0x02*channel + 0x00;
        off_t step_offset = stream->offset + 0x02*channel + 0x02*vgmstream->channels;

        hist1   = read_16bitLE(hist_offset,stream->streamfile);
        step_index = read_8bit(step_offset,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;

        /* write header sample (even samples per block, skips last nibble) */
        outbuf[sample_count] = (short)(hist1);
        sample_count += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    /* decode nibbles (layout: 2 bytes/2*2 nibbles per channel) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        off_t byte_offset = stream->offset + 0x04*vgmstream->channels + 0x02*channel + (i-1)/4*2*vgmstream->channels + ((i-1)%4)/2;
        int nibble_shift = ((i-1)&1?4:0); /* low nibble first */

        /* must skip last nibble per official decoder, probably not needed though */
        if (i < block_samples) {
            std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count += channelspacing;
        }
    }

    /* internal interleave: increment offset on complete frame */
    if (i == block_samples) {
        stream->offset += 0x24*vgmstream->channels;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* mono XBOX-IMA with header endianness and alt nibble expand (verified vs AK test demos) */
void decode_wwise_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count = 0, num_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    /* external interleave (fixed size), mono */
    int block_samples = (0x24 - 0x4) * 2;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    /* normal header (hist+step+reserved), single channel */
    if (first_sample == 0) {
        int16_t (*read_16bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_16bitBE : read_16bitLE;
        off_t header_offset = stream->offset + 0x24*num_frame;

        hist1 = read_16bit(header_offset,stream->streamfile);
        step_index = read_8bit(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;

        /* write header sample (even samples per block, skips last nibble) */
        outbuf[sample_count] = (short)(hist1);
        sample_count += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    /* decode nibbles (layout: all nibbles from one channel) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        off_t byte_offset = (stream->offset + 0x24*num_frame + 0x4) + (i-1)/2;
        int nibble_shift = ((i-1)&1?4:0); /* low nibble first */

        /* must skip last nibble like other XBOX-IMAs, often needed (ex. Bayonetta 2 sfx) */
        if (i < block_samples) {
            std_ima_expand_nibble_mul(stream, byte_offset,nibble_shift, &hist1, &step_index);
            outbuf[sample_count] = (short)(hist1);
            sample_count += channelspacing;
        }
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* MS-IMA with possibly the XBOX-IMA model of even number of samples per block (more tests are needed) */
void decode_awc_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave, mono
    int block_samples = (0x800 - 4) * 2;
    first_sample = first_sample % block_samples;

    //inverted header
    if (first_sample == 0) {
        off_t header_offset = stream->offset;

        step_index = read_16bitLE(header_offset,stream->streamfile);
        hist1 = read_16bitLE(header_offset+2,stream->streamfile);
        if (step_index < 0) step_index=0;
        if (step_index > 88) step_index=88;
    }

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t byte_offset = stream->offset + 4 + i/2;
        int nibble_shift = (i&1?4:0); //low nibble first

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1);
    }

    //internal interleave: increment offset on complete frame
    if (i == block_samples) stream->offset += 0x800;

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


/* DVI stereo/mono with some mini header and sample output */
void decode_ubi_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave

    //header in the beginning of the stream
    if (stream->channel_start_offset == stream->offset) {
        int version, big_endian, header_samples, max_samples_to_do;
        int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
        off_t offset = stream->offset;

        /* header fields mostly unknown (vary a lot or look like flags, tool version?, 0x08: stereo flag?) */
        version = read_8bit(offset + 0x00, stream->streamfile);
        big_endian = version < 5;
        read_16bit = big_endian ? read_16bitBE : read_16bitLE;

        header_samples = read_16bit(offset + 0x0E, stream->streamfile); /* always 10 (per channel) */
        hist1      = read_16bit(offset + 0x10 + channel*0x04,stream->streamfile);
        step_index =  read_8bit(offset + 0x12 + channel*0x04,stream->streamfile);
        offset += 0x10 + 0x08;
        if (version >= 3)
            offset += 0x04;
        if (version >= 6) /* later BAOs */
            offset += 0x08;

        /* write PCM samples, must be written to match header's num_samples (hist mustn't) */
        max_samples_to_do = ((samples_to_do > header_samples) ? header_samples : samples_to_do);
        for (i = first_sample; i < max_samples_to_do; i++, sample_count += channelspacing) {
            outbuf[sample_count] = read_16bit(offset + channel*sizeof(sample) + i*channelspacing*sizeof(sample),stream->streamfile);
            first_sample++;
            samples_to_do--;
        }

        /* header done */
        if (i == header_samples) {
            stream->offset = offset + header_samples*channelspacing*sizeof(sample);
        }
    }

    first_sample -= 10; //todo fix hack (needed to adjust nibble offset below)

    if (step_index < 0) step_index = 0;
    if (step_index > 88) step_index = 88;

    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = channelspacing == 1 ?
                stream->offset + i/2 :  /* mono mode */
                stream->offset + i;     /* stereo mode */
        int nibble_shift = channelspacing == 1 ?
                (!(i%2) ? 4:0) :        /* mono mode (high first) */
                (channel==0 ? 4:0);     /* stereo mode (high=L,low=R) */

        std_ima_expand_nibble_mul(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1); /* all samples are written */
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* standard IMA but with a tweak for Ubi's encoder bug with step index (see blocked_ubi_sce.c) */
void decode_ubi_sce_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count = 0;

    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;

    //internal interleave

    if (step_index < 0) step_index = 0;
    if (step_index > 89) step_index = 89;

    for (i = first_sample; i < first_sample + samples_to_do; i++, sample_count += channelspacing) {
        off_t byte_offset = channelspacing == 1 ?
                stream->offset + i/2 :  /* mono mode */
                stream->offset + i;     /* stereo mode */
        int nibble_shift = channelspacing == 1 ?
                (!(i%2) ? 4:0) :        /* mono mode (high first) */
                (channel==0 ? 4:0);     /* stereo mode (high=L,low=R) */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);
        outbuf[sample_count] = (short)(hist1); /* all samples are written */
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


/* IMA with variable frame formats controlled by the block layout. The original code uses
 * tables mapping all standard IMA combinations (to optimize calculations), but decodes the same.
 * Based on HCS's and Nisto's reverse engineering in h4m_audio_decode. */
void decode_h4m_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, uint16_t frame_format) {
    int i, samples_done = 0;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;
    size_t header_size;
    int is_stereo = (channelspacing > 1);

    /* external interleave (blocked, should call 1 frame) */

    /* custom header, per channel */
    if (first_sample == 0) {
        int channel_pos = is_stereo ? (1 - channel) : channel; /* R hist goes first */
        switch(frame_format) {
            case 1: /* combined hist+index */
                hist1   = read_16bitBE(stream->offset + 0x02*channel_pos + 0x00,stream->streamfile) & 0xFFFFFF80;
                step_index = (uint8_t)read_8bit(stream->offset + 0x02*channel_pos + 0x01,stream->streamfile) & 0x7f;
                break;
            case 3: /* separate hist+index */
                hist1   = read_16bitBE(stream->offset + 0x03*channel_pos + 0x00,stream->streamfile);
                step_index = (uint8_t)read_8bit(stream->offset + 0x03*channel_pos + 0x02,stream->streamfile);
                break;
            case 2:  /* no hist/index (continues from previous frame) */
            default:
                break;
        }

        /* write header sample (last nibble is skipped) */
        if (frame_format == 1 || frame_format == 3) {
            outbuf[samples_done * channelspacing] = (short)hist1;
            samples_done++;
            samples_to_do--;
        }

        /* clamp corrupted data just in case */
        if (step_index < 0) step_index = 0;
        if (step_index > 88) step_index = 88;
    }
    else {
        /* offset adjust for header sample */
        if (frame_format == 1 || frame_format == 3) {
            first_sample--;
        }
    }

    /* offset adjust */
    switch(frame_format) {
        case 1: header_size = (channelspacing*0x02); break;
        case 3: header_size = (channelspacing*0x03); break;
        default: header_size = 0; break;
    }

    /* decode block nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        off_t byte_offset = is_stereo ?
                stream->offset + header_size + i :      /* stereo: one nibble per channel */
                stream->offset + header_size + i/2;     /* mono: consecutive nibbles */
        int nibble_shift = is_stereo ?
                (!(channel&1) ? 0:4) :                  /* stereo: L=low, R=high */
                (!(i&1) ? 0:4);                         /* mono: low first */

        std_ima_expand_nibble(stream, byte_offset,nibble_shift, &hist1, &step_index);

        outbuf[samples_done * channelspacing] = (short)(hist1);
        samples_done++;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}

/* test... */
static inline int _clamp_s32(int value, int min, int max) {
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

/* Crystal Dynamics IMA. Original code uses mind-bending intrinsics, so this may not be fully accurate.
 * Has another table with delta_table MMX combos, and uses header sample (first nibble is always 0). */
void decode_cd_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x24] = {0};
    int i, frames_in, sample_pos = 0, block_samples, frame_size;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_index = stream->adpcm_step_index;
    off_t frame_offset;

    /* external interleave (fixed size), mono */
    frame_size = 0x24;
    block_samples = (frame_size - 0x4) * 2;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    frame_offset = stream->offset + frame_size * frames_in;
    read_streamfile(frame, frame_offset, frame_size, stream->streamfile); /* ignore EOF errors */

    /* normal header (hist+step+reserved), mono */
    if (first_sample == 0) {
        hist1   = get_s16le(frame + 0x00);
        step_index = get_u8(frame + 0x02);
        step_index = _clamp_s32(step_index, 0, 88);

        /* write header sample (even samples per block, skips first nibble) */
        outbuf[sample_pos] = (short)(hist1);
        sample_pos += channelspacing;
        first_sample += 1;
        samples_to_do -= 1;
    }

    /* decode nibbles (layout: straight in mono) */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int pos = 0x04 + (i/2);
        int shift = (i&1 ? 4:0); /* low first, but first low nibble is skipped */

        cd_ima_expand_nibble(frame[pos], shift, &hist1, &step_index);
        outbuf[sample_pos] = (short)(hist1);
        sample_pos += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_index;
}


/* ************************************************************* */

size_t ima_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    /* 2 samples per byte (2 nibbles) in stereo or mono config */
    return bytes * 2 / channels;
}

size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels) {
    if (block_align <= 0 || channels <= 0) return 0;
    /* MS-IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * ((block_align - 0x04*channels) * 2 / channels + 1)
            + ((bytes % block_align) ? (((bytes % block_align) - 0x04*channels) * 2 / channels + 1) : 0);
}

size_t xbox_ima_bytes_to_samples(size_t bytes, int channels) {
    int mod;
    int block_align = 0x24 * channels;
    if (channels <= 0) return 0;

    mod = bytes % block_align;
    /* XBOX IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * (block_align - 4 * channels) * 2 / channels
            + ((mod > 0 && mod > 0x04*channels) ? (mod - 0x04*channels) * 2 / channels : 0); /* unlikely (encoder aligns) */
}

size_t dat4_ima_bytes_to_samples(size_t bytes, int channels) {
    int block_align = 0x20 * channels;
    if (channels <= 0) return 0;
    /* DAT4 IMA blocks have a 4 byte header per channel; 2 samples per byte (2 nibbles) */
    return (bytes / block_align) * (block_align - 4 * channels) * 2 / channels
            + ((bytes % block_align) ? ((bytes % block_align) - 4 * channels) * 2 / channels : 0); /* unlikely (encoder aligns) */
}

size_t apple_ima4_bytes_to_samples(size_t bytes, int channels) {
    int block_align = 0x22 * channels;
    if (channels <= 0) return 0;
    return (bytes / block_align) * (block_align - 0x02*channels) * 2 / channels
            + ((bytes % block_align) ? ((bytes % block_align) - 0x02*channels) * 2 / channels : 0);
}


/* test XBOX-ADPCM frames for correctness */
int xbox_check_format(STREAMFILE* sf, uint32_t offset, uint32_t max, int channels) {
    off_t max_offset = offset + max;
    int ch;

    if (max_offset > get_streamfile_size(sf))
        max_offset = get_streamfile_size(sf);
    if (!channels)
        return 0;

    while (offset < max_offset) {
        for (ch = 0; ch < channels; ch++) {
            uint16_t step = read_u16le(offset + 0x04 * ch + 0x02,sf);
            if (step > 88)
                return 0;
        }

        offset += 0x24 * channels;
    }

    return 1;
}
