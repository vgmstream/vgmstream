#include "coding.h"


/* Decodes Ubisoft ADPCM, a rather complex codec with 4-bit (usually music) and 6-bit (usually voices/sfx)
 * mono or stereo modes, using multiple tables and temp step/delta values.
 *
 * Base reverse engineering by Zench: https://bitbucket.org/Zenchreal/decubisnd
 * Original 4-bit mode ASM MMX/intrinsics to C++ by sigsegv; adapted + 6-bit mode by bnnm; special thanks to Nicknine.
 *
 * Data always starts with a 0x30 main header (some games have extra data before too), then frames of
 * fixed size: 0x34 ADPCM setup per channel, 1 subframe + 1 padding byte, then another subframe and 1 byte.
 * Subframes have 1536 samples or less (like 1024), typical sizes are 0x600 for 4-bit or 0x480 for 6-bit.
 * Last frame can contain only one subframe, with less codes than normal (may use padding). Nibbles/codes
 * are packed as 32-bit LE with 6-bit or 4-bit codes for all channels (processes kinda like joint stereo).
 */

#define UBI_CHANNELS_MIN                1
#define UBI_CHANNELS_MAX                2
#define UBI_SUBFRAMES_PER_FRAME_MAX     2
#define UBI_CODES_PER_SUBFRAME_MAX      1536 /* for all channels */
#define UBI_FRAME_SIZE_MAX              (0x34 * UBI_CHANNELS_MAX + (UBI_CODES_PER_SUBFRAME_MAX * 6 / 8 + 0x1) * UBI_SUBFRAMES_PER_FRAME_MAX)
#define UBI_SAMPLES_PER_FRAME_MAX       (UBI_CODES_PER_SUBFRAME_MAX * UBI_SUBFRAMES_PER_FRAME_MAX)


typedef struct {
    uint32_t signature;
    uint32_t sample_count;
    uint32_t subframe_count;
    uint32_t codes_per_subframe_last;
    uint32_t codes_per_subframe;
    uint32_t subframes_per_frame;
    uint32_t unknown18;
    uint32_t unknown1c;
    uint32_t unknown20;
    uint32_t bits_per_sample;
    uint32_t unknown28;
    uint32_t channels;
} ubi_adpcm_header_data;

typedef struct {
    uint32_t signature;
    int32_t step1;
    int32_t next1;
    int32_t next2;

    int16_t coef1;
    int16_t coef2;
    int16_t unused1;
    int16_t unused2;

    int16_t mod1;
    int16_t mod2;
    int16_t mod3;
    int16_t mod4;

    int16_t hist1;
    int16_t hist2;
    int16_t unused3;
    int16_t unused4;

    int16_t delta1;
    int16_t delta2;
    int16_t delta3;
    int16_t delta4;

    int16_t delta5;
    int16_t unused5;
} ubi_adpcm_channel_data;

struct ubi_adpcm_codec_data {
    ubi_adpcm_header_data header;
    ubi_adpcm_channel_data ch[UBI_CHANNELS_MAX];

    uint32_t start_offset;
    uint32_t offset;
    int subframe_number;

    uint8_t frame[UBI_FRAME_SIZE_MAX];
    uint8_t codes[UBI_CODES_PER_SUBFRAME_MAX];
    int16_t samples[UBI_SAMPLES_PER_FRAME_MAX]; /* for all channels, saved in L-R-L-R form */

    size_t samples_filled;
    size_t samples_consumed;
    size_t samples_to_discard;
};

/* *********************************************************************** */

static int parse_header(STREAMFILE* sf, ubi_adpcm_codec_data* data, uint32_t offset, uint32_t size);
static void decode_frame(STREAMFILE* sf, ubi_adpcm_codec_data* data);
static void fix_samples(ubi_adpcm_codec_data* data, uint32_t size);

ubi_adpcm_codec_data* init_ubi_adpcm(STREAMFILE* sf, uint32_t offset, uint32_t size, int channels) {
    ubi_adpcm_codec_data* data = NULL;

    data = calloc(1, sizeof(ubi_adpcm_codec_data));
    if (!data) goto fail;

    if (!parse_header(sf, data, offset, size)) {
        VGM_LOG("UBI ADPCM: wrong header\n");
        goto fail;
    }

    if (data->header.channels != channels) {
        VGM_LOG("UBI ADPCM: wrong number of channels: %i vs %i\n", data->header.channels, channels);
        goto fail;
    }

    data->start_offset = offset + 0x30;
    data->offset = data->start_offset;

    return data;
fail:
    free_ubi_adpcm(data);
    return NULL;
}

void decode_ubi_adpcm(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    ubi_adpcm_codec_data* data = vgmstream->codec_data;
    uint32_t channels = data->header.channels;
    int samples_done = 0;


    /* Ubi ADPCM frames are rather big, so we decode then copy to outbuf until done */
    while (samples_done < samples_to_do) {
        if (data->samples_filled) {
            int samples_to_get = data->samples_filled;

            if (data->samples_to_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_to_discard)
                    samples_to_get = data->samples_to_discard;
                data->samples_to_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do - samples_done)
                    samples_to_get = samples_to_do - samples_done;

                memcpy(outbuf + samples_done*channels,
                       data->samples + data->samples_consumed*channels,
                       samples_to_get*channels * sizeof(sample));
                samples_done += samples_to_get;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
            data->samples_filled -= samples_to_get;
        }
        else {
            decode_frame(sf, data);
        }
    }
}

void reset_ubi_adpcm(ubi_adpcm_codec_data* data) {
    if (!data) return;

    data->offset = data->start_offset;
    data->subframe_number = 0;
}

void seek_ubi_adpcm(ubi_adpcm_codec_data* data, int32_t num_sample) {
    if (!data) return;

    //todo improve by seeking to closest frame

    reset_ubi_adpcm(data);
    data->samples_to_discard = num_sample;
}

void free_ubi_adpcm(ubi_adpcm_codec_data *data) {
    if (!data)
        return;
    free(data);
}


/* ************************************************************************ */

static void read_header_state(uint8_t* data, ubi_adpcm_header_data* header) {
    header->signature              = get_u32le(data + 0x00);
    header->sample_count           = get_u32le(data + 0x04);
    header->subframe_count         = get_u32le(data + 0x08);
    header->codes_per_subframe_last= get_u32le(data + 0x0c);
    header->codes_per_subframe     = get_u32le(data + 0x10);
    header->subframes_per_frame    = get_u32le(data + 0x14);
    header->unknown18              = get_u32le(data + 0x18); /* sometimes sample rate but algo other values (garbage?) */
    header->unknown1c              = get_u32le(data + 0x1c); /* variable */
    header->unknown20              = get_u32le(data + 0x20); /* null? */
    header->bits_per_sample        = get_u32le(data + 0x24);
    header->unknown28              = get_u32le(data + 0x28); /* 1~3? */
    header->channels               = get_u32le(data + 0x2c);
}

static int parse_header(STREAMFILE* sf, ubi_adpcm_codec_data* data, uint32_t offset, uint32_t size) {
    uint8_t buf[0x30];
    size_t bytes;

    bytes = read_streamfile(buf, offset, 0x30, sf);
    if (bytes != 0x30) goto fail;

    read_header_state(buf, &data->header);

    if (data->header.signature != 0x08)
        goto fail;
    if (data->header.codes_per_subframe_last > UBI_CODES_PER_SUBFRAME_MAX ||
        data->header.codes_per_subframe > UBI_CODES_PER_SUBFRAME_MAX)
        goto fail;
    if (data->header.subframes_per_frame != UBI_SUBFRAMES_PER_FRAME_MAX)
        goto fail;
    if (data->header.bits_per_sample != 4 && data->header.bits_per_sample != 6)
        goto fail;
    if (data->header.channels > UBI_CHANNELS_MAX || data->header.channels < UBI_CHANNELS_MIN)
        goto fail;

    /* some kind of internal bug I guess, seen in a few subsongs in Rayman 3 PC demo */
    if (data->header.sample_count == 0x77E7A374 * data->header.channels) {
        fix_samples(data, size);
    }

    return 1;
fail:
    return 0;
}

/* *********************************************************************** */

static const int32_t adpcm6_table1[64] = {
        -100000000, -369, -245, -133, -33, 56, 135, 207,
        275, 338, 395, 448, 499, 548, 593, 635,
        676, 717, 755, 791, 825, 858, 889, 919,
        948, 975, 1003, 1029, 1054, 1078, 1103, 1132,
        /* probably unused (partly spilled from next table) */
        1800,   1800,  1800,  2048,  3072,  4096,   5000,  5056,
        5184,   5240,  6144,  6880,  9624, 12880,  14952, 18040,
        20480, 22920, 25600, 28040, 32560, 35840,  40960, 45832,
        51200, 56320, 63488, 67704, 75776, 89088, 102400,     0,
};

static const int32_t adpcm6_table2[64] = {
        1800,   1800,  1800,  2048,  3072,  4096,   5000,  5056,
        5184,   5240,  6144,  6880,  9624, 12880,  14952, 18040,
        20480, 22920, 25600, 28040, 32560, 35840,  40960, 45832,
        51200, 56320, 63488, 67704, 75776, 89088, 102400,     0,
        /* probably unused */
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 2, 2, 3, 3, 4,
        4, 5, 5, 5, 6, 6, 6, 7,
};

static const int32_t adpcm4_table1[16] = {
        -100000000, 8, 269, 425, 545, 645, 745, 850,
        /* probably unused */
        -1082465976, 1058977874, 1068540887, 1072986849, 1075167887, 1076761723, 1078439444, 1203982336,
};

static const int32_t adpcm4_table2[16] = {
        -1536, 2314, 5243, 8192, 14336, 25354, 45445, 143626,
        /* probably unused */
        0, 0, 0, 1, 1, 1, 3, 7,
};

static const int32_t delta_table[33+33] = {
        1024, 1031, 1053, 1076, 1099, 1123, 1148, 1172,
        1198, 1224, 1251, 1278, 1306, 1334, 1363, 1393,
        1423, 1454, 1485, 1518, 1551, 1584, 1619, 1654,
        1690, 1726, 1764, 1802, 1841, 1881, 1922, 1964,
        2007,
       -1024,-1031,-1053,-1076,-1099,-1123,-1148,-1172,
       -1198,-1224,-1251,-1278,-1306,-1334,-1363,-1393,
       -1423,-1454,-1485,-1518,-1551,-1584,-1619,-1654,
       -1690,-1726,-1764,-1802,-1841,-1881,-1922,-1964,
       -2007
};


static int sign16(int16_t test) {
    return (test < 0 ? -1 : 1);
}
static int sign32(int32_t test) {
    return (test < 0 ? -1 : 1);
}
static int16_t absmax16(int16_t val, int16_t absmax) {
    if (val < 0) {
        if (val < -absmax) return -absmax;
    } else {
        if (val > absmax) return absmax;
    }
    return val;
}
static int32_t clamp_val(int32_t val, int32_t min, int32_t max) {
    if (val < min) return min;
    else if (val > max) return max;
    else return val;
}


static int16_t expand_code_6bit(uint8_t code, ubi_adpcm_channel_data* state) {
    int step0_index;
    int32_t code_signed, step0_next, step0, delta0;
    int32_t sample_new;

    code_signed = (int32_t)code - 31; /* convert coded 0..63 value to signed value, where 0=-31 .. 31=0 .. 63=32 */
    step0_index = abs(code_signed); /* 0..32, but should only go up to 31 */
    step0_next = adpcm6_table1[step0_index] + state->step1;
    step0 = (state->step1 & 0xFFFF) * 246;
    step0 = (step0 + adpcm6_table2[step0_index]) >> 8;
    step0 = clamp_val(step0, 271, 2560);

    delta0 = 0;
    if (!(((step0_next & 0xFFFFFF00) - 1) & (1 << 31))) {
        int delta0_index = ((step0_next >> 3) & 0x1F) + (code_signed < 0 ? 33 : 0);
        int delta0_shift = clamp_val((step0_next >> 8) & 0xFF, 0,31);
        delta0 = (delta_table[delta0_index] << delta0_shift) >> 10;
    }

    sample_new = (int16_t)(delta0 + state->delta1 + state->hist1);

    state->hist1 = sample_new;

    state->step1 = step0;

    state->delta1 = delta0;

    VGM_ASSERT(step0_index > 31, "UBI ADPCM: index over 31\n");
    return sample_new;
}

/* may be simplified (masks, saturation, etc) as some values should never happen in the encoder */
static int16_t expand_code_4bit(uint8_t code, ubi_adpcm_channel_data* state) {
    int step0_index;
    int32_t code_signed, step0_next, step0, delta0, next0, coef1_next, coef2_next;
    int32_t sample_new;


    code_signed = (int32_t)code - 7; /* convert coded 0..15 value to signed value, where 0=-7 .. 7=0 .. 15=8 */
    step0_index = abs(code_signed); /*  0..8, but should only go up to 7 */
    step0_next = adpcm4_table1[step0_index] + state->step1;
    step0 = (state->step1 & 0xFFFF) * 246;
    step0 = (step0 + adpcm4_table2[step0_index]) >> 8;
    step0 = clamp_val(step0, 271, 2560);

    delta0 = 0;
    if (!(((step0_next & 0xFFFFFF00) - 1) & (1 << 31))) {
        int delta0_index = ((step0_next >> 3) & 0x1F) + (code_signed < 0 ? 33 : 0);
        int delta0_shift = clamp_val((step0_next >> 8) & 0xFF, 0,31);
        delta0 = (delta_table[delta0_index] << delta0_shift) >> 10;
    }

    next0 = (int16_t)((
            (state->mod1 * state->delta1) + (state->mod2 * state->delta2) +
            (state->mod3 * state->delta3) + (state->mod4 * state->delta4) ) >> 10);

    sample_new = ((state->coef1 * state->hist1) + (state->coef2 * state->hist2)) >> 10;
    sample_new = (int16_t)(delta0 + next0 + sample_new);

    coef1_next = state->coef1 * 255;
    coef2_next = state->coef2 * 254;
    delta0 = (int16_t)delta0;
    if (delta0 + next0 != 0) {
        int32_t sign1, sign2, coef_delta;

        sign1 = sign32(delta0 + next0) * sign32(state->delta1 + state->next1);
        sign2 = sign32(delta0 + next0) * sign32(state->delta2 + state->next2);

        coef_delta = (int16_t)((((sign1 * 3072) + coef1_next) >> 6) & ~0x3);
        coef_delta = clamp16(clamp16(coef_delta + 30719) - 30719); //???
        coef_delta = clamp16(clamp16(coef_delta + -30720) - -30720); //???
        coef_delta = ((int16_t)(sign2 * 1024) - (int16_t)(sign1 * coef_delta)) * 2;

        coef1_next += sign1 * 3072;
        coef2_next += coef_delta;
    }


    state->hist2 = state->hist1;
    state->hist1 = sample_new;

    state->coef2 = absmax16((int16_t)(coef2_next >> 8), 768);
    state->coef1 = absmax16((int16_t)(coef1_next >> 8), 960 - state->coef2);

    state->next2 = state->next1;
    state->next1 = next0;
    state->step1 = step0;

    state->delta5 = state->delta4;
    state->delta4 = state->delta3;
    state->delta3 = state->delta2;
    state->delta2 = state->delta1;
    state->delta1 = delta0;

    state->mod4 = clamp16(state->mod4 * 255 + 2048 * sign16(state->delta1) * sign16(state->delta5)) >> 8;
    state->mod3 = clamp16(state->mod3 * 255 + 2048 * sign16(state->delta1) * sign16(state->delta4)) >> 8;
    state->mod2 = clamp16(state->mod2 * 255 + 2048 * sign16(state->delta1) * sign16(state->delta3)) >> 8;
    state->mod1 = clamp16(state->mod1 * 255 + 2048 * sign16(state->delta1) * sign16(state->delta2)) >> 8;

    VGM_ASSERT(step0_index > 7, "UBI ADPCM: index over 7\n");
    return sample_new;
}

static void decode_subframe_mono(ubi_adpcm_channel_data* ch_state, uint8_t* codes, int16_t* samples, int code_count, int bps) {
    int i;
    int16_t (*expand_code)(uint8_t,ubi_adpcm_channel_data*) = NULL;

    if (bps == 6)
        expand_code = expand_code_6bit;
    else
        expand_code = expand_code_4bit;

    for (i = 0; i < code_count; i++) {
        samples[i] = expand_code(codes[i], ch_state);
    }
}

static void decode_subframe_stereo(ubi_adpcm_channel_data* ch0_state, ubi_adpcm_channel_data* ch1_state, uint8_t* codes, int16_t* samples, int code_count, int bps) {
    int i;
    int16_t (*expand_code)(uint8_t,ubi_adpcm_channel_data*) = NULL;

    if (bps == 6)
        expand_code = expand_code_6bit;
    else
        expand_code = expand_code_4bit;

    for (i = 0; i < code_count; i += 8) {
        samples[i + 0] = expand_code(codes[i + 0], ch0_state);
        samples[i + 1] = expand_code(codes[i + 2], ch0_state);
        samples[i + 2] = expand_code(codes[i + 4], ch0_state);
        samples[i + 3] = expand_code(codes[i + 6], ch0_state);
        samples[i + 4] = expand_code(codes[i + 1], ch1_state);
        samples[i + 5] = expand_code(codes[i + 3], ch1_state);
        samples[i + 6] = expand_code(codes[i + 5], ch1_state);
        samples[i + 7] = expand_code(codes[i + 7], ch1_state);
    }

    for (i = 0; i < code_count; i += 8) {
        int16_t samples_old[8];
        memcpy(samples_old, samples, sizeof(samples_old));

        samples[0] = clamp16(samples_old[0] + samples_old[4]);
        samples[1] = clamp16(samples_old[0] - samples_old[4]);
        samples[2] = clamp16(samples_old[1] + samples_old[5]);
        samples[3] = clamp16(samples_old[1] - samples_old[5]);

        samples[4] = clamp16(samples_old[2] + samples_old[6]);
        samples[5] = clamp16(samples_old[2] - samples_old[6]);
        samples[6] = clamp16(samples_old[3] + samples_old[7]);
        samples[7] = clamp16(samples_old[3] - samples_old[7]);

        samples += 8;
    }
}

/* unpack uint32 LE data into 4/6-bit codes:
 * - for 4-bit, 32b contain 8 codes
 *    ex. uint8_t 0x98576787DB5725A8... becomes 0x87675798 LE = 8 7 6 7 5 7 9 8 ...
 * - for 6-bit, 32b contain ~5 codes with leftover bits used in following 32b
 *    ex. uint8_t 0x98576787DB5725A8... becomes 0x87675798 LE = 100001 110110 011101 010111 100110 00,
 *    0xA82557DB LE = 1010 100000 100101 010101 111101 1011 ... (where last 00 | first 1010 = 001010), etc
 * Codes aren't signed but rather have a particular meaning (see decoding).
 */
static void unpack_codes(uint8_t* data, uint8_t* codes, int code_count, int bps) {
    int i;
    size_t pos = 0;
    uint64_t bits = 0, input = 0;
    const uint64_t mask = (bps == 6) ? 0x3f : 0x0f;

    for (i = 0; i < code_count; i++) {
        if (bits < bps) {
            uint32_t source32le = get_u32le(data + pos);
            pos += 0x04;

            input = (input << 32) | (uint64_t)source32le;
            bits += 32;
        }

        bits -= bps;
        codes[i] = (uint8_t)((input >> bits) & mask);
    }
}

static void read_channel_state(uint8_t* data, ubi_adpcm_channel_data* ch) {
    /* ADPCM frame state, some fields are unused and contain repeated garbage in all frames but
     * probably exist for padding (original code uses MMX to operate in multiple 16b at the same time)
     * or reserved for other bit modes */

    ch->signature   = get_u32le(data + 0x00);
    ch->step1       = get_s32le(data + 0x04);
    ch->next1       = get_s32le(data + 0x08);
    ch->next2       = get_s32le(data + 0x0c);

    ch->coef1       = get_s16le(data + 0x10);
    ch->coef2       = get_s16le(data + 0x12);
    ch->unused1     = get_s16le(data + 0x14);
    ch->unused2     = get_s16le(data + 0x16);
    ch->mod1        = get_s16le(data + 0x18);
    ch->mod2        = get_s16le(data + 0x1a);
    ch->mod3        = get_s16le(data + 0x1c);
    ch->mod4        = get_s16le(data + 0x1e);

    ch->hist1       = get_s16le(data + 0x20);
    ch->hist2       = get_s16le(data + 0x22);
    ch->unused3     = get_s16le(data + 0x24);
    ch->unused4     = get_s16le(data + 0x26);
    ch->delta1      = get_s16le(data + 0x28);
    ch->delta2      = get_s16le(data + 0x2a);
    ch->delta3      = get_s16le(data + 0x2c);
    ch->delta4      = get_s16le(data + 0x2e);

    ch->delta5      = get_s16le(data + 0x30);
    ch->unused5     = get_s16le(data + 0x32);

    VGM_ASSERT(ch->signature != 0x02,  "UBI ADPCM: incorrect channel header\n");
    VGM_ASSERT(ch->unused3 != 0x00,    "UBI ADPCM: found unused3 used\n");
    VGM_ASSERT(ch->unused4 != 0x00,    "UBI ADPCM: found unused4 used\n");
}

static void decode_frame(STREAMFILE* sf, ubi_adpcm_codec_data* data) {
    int code_count_a, code_count_b;
    size_t subframe_size_a, subframe_size_b, frame_size, bytes;
    int bps = data->header.bits_per_sample;
    int channels = data->header.channels;


    /* last frame is shorter (subframe A or B may not exist), avoid over-reads in bigfiles */
    if (data->subframe_number + 1 == data->header.subframe_count) {
        code_count_a = data->header.codes_per_subframe_last;
        code_count_b = 0;
    } else if (data->subframe_number + 2 == data->header.subframe_count) {
        code_count_a = data->header.codes_per_subframe;
        code_count_b = data->header.codes_per_subframe_last;
    } else {
        code_count_a = data->header.codes_per_subframe;
        code_count_b = data->header.codes_per_subframe;
    }

    subframe_size_a = (bps * code_count_a / 8);
    if (subframe_size_a) subframe_size_a += 0x01;
    subframe_size_b = (bps * code_count_b / 8);
    if (subframe_size_b) subframe_size_b += 0x01;

    frame_size = 0x34 * channels + subframe_size_a + subframe_size_b;

    //todo check later games (ex. Myst IV) if they handle this
    /* last frame can have an odd number of codes, with data ending not aligned to 32b,
     * but RE'd code unpacking and stereo decoding always assume to be aligned, causing clicks in some cases
     * (if data ends in 0xEE it'll try to do 0x000000EE, but only unpack codes 0 0, thus ignoring actual last 2) */
    //memset(data->frame, 0, sizeof(data->frame));
    //memset(data->codes, 0, sizeof(data->codes));
    //memset(data->samples, 0, sizeof(data->samples));


    bytes = read_streamfile(data->frame, data->offset, frame_size, sf);
    if (bytes != frame_size) {
        VGM_LOG("UBI ADPCM: wrong bytes read %x vs %x at %x\n", bytes, frame_size, data->offset);
        //goto fail; //may reach EOF earlier
    }

    if (channels == 1) {
        read_channel_state(data->frame + 0x00, &data->ch[0]);

        unpack_codes(data->frame + 0x34, data->codes, code_count_a, bps);
        decode_subframe_mono(&data->ch[0], data->codes, &data->samples[0], code_count_a, bps);

        unpack_codes(data->frame + 0x34 + subframe_size_a, data->codes, code_count_b, bps);
        decode_subframe_mono(&data->ch[0], data->codes, &data->samples[code_count_a], code_count_b, bps);
    }
    else if (channels == 2) {
        read_channel_state(data->frame + 0x00, &data->ch[0]);
        read_channel_state(data->frame + 0x34, &data->ch[1]);

        unpack_codes(data->frame + 0x68, data->codes, code_count_a, bps);
        decode_subframe_stereo(&data->ch[0], &data->ch[1], data->codes, &data->samples[0], code_count_a, bps);

        unpack_codes(data->frame + 0x68 + subframe_size_a, data->codes, code_count_b, bps);
        decode_subframe_stereo(&data->ch[0], &data->ch[1], data->codes, &data->samples[code_count_a], code_count_b, bps);
    }

    /* frame done */
    data->offset += frame_size;
    data->subframe_number += 2;
    data->samples_consumed = 0;
    data->samples_filled = (code_count_a + code_count_b) / channels;
}


int32_t ubi_adpcm_get_samples(ubi_adpcm_codec_data* data) {
    if (!data)
        return 0;

    return data->header.sample_count / data->header.channels;
}

static void fix_samples(ubi_adpcm_codec_data* data, uint32_t size) {
    uint32_t frame_size, setup_size, subframe_size, base_frames, last_size;
    int subframes;
    int32_t samples;

    if (!data || !data->header.channels || !data->header.subframes_per_frame || !size)
        return;

    size -= 0x30; /* ignore header */

    setup_size = 0x34 * data->header.channels; /* setup per channel */
    subframe_size = (data->header.codes_per_subframe * data->header.bits_per_sample /*+ 8*/) / 8 + 0x01; /* padding byte */
    frame_size = setup_size + subframe_size * data->header.subframes_per_frame;

    /* don't trust subframe count */
    base_frames = ((size - 0x01) / frame_size); /* force smaller size just in case so last frame isn't used */
    last_size = size - (base_frames * frame_size);
    subframes = base_frames * data->header.subframes_per_frame;

    samples = base_frames * (data->header.codes_per_subframe * data->header.subframes_per_frame);

    /* last subframe is shorter (and may contain padding after codes_per_subframe_last), and last frame may not contain all subframes */
    if (last_size > setup_size + subframe_size) {
        samples += data->header.codes_per_subframe * (data->header.subframes_per_frame - 1);
        subframes += (data->header.subframes_per_frame - 1);
    }

    /* for some reason several files that need fixing seem to have garbage in the 2nd half of last codes */
    samples += data->header.codes_per_subframe_last / 2;
    subframes += 1;

    data->header.sample_count = samples; /* for all channels */
    data->header.subframe_count = subframes;
}
