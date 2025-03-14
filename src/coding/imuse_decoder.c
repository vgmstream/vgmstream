#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "../util/bitstream_msb.h"

/* LucasArts' iMUSE decoder, mainly for VIMA (like IMA but with variable frame and code sizes).
 * Reverse engineered from various .exe
 *
 * Info:
 * - https://github.com/scummvm/scummvm/blob/master/engines/scumm/imuse_digi/dimuse_codecs.cpp (V1)
 * - https://wiki.multimedia.cx/index.php/VIMA (V2)
 * - https://github.com/residualvm/residualvm/tree/master/engines/grim/imuse
 *   https://github.com/residualvm/residualvm/tree/master/engines/grim/movie/codecs (V2)
 */

static const int16_t step_table[] = { /* same as IMA */
    7,    8,    9,    10,   11,   12,   13,   14,
    16,   17,   19,   21,   23,   25,   28,   31,
    34,   37,   41,   45,   50,   55,   60,   66,
    73,   80,   88,   97,   107,  118,  130,  143,
    157,  173,  190,  209,  230,  253,  279,  307,
    337,  371,  408,  449,  494,  544,  598,  658,
    724,  796,  876,  963,  1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794,
    32767,
};

/* pre-calculated in V1:
    for (i = 0; i < 89; i++) {
       int counter = (4 * step_table[i] / 7) >> 1;
       int size = 1;
       while (counter > 0) {
           size++;
           counter >>= 1;
       }
       code_size_table[i] = clamp(size, 3, 8) - 1
    }
*/
static const uint8_t code_size_table_v1[89] = {
    2, 2, 2, 2, 2, 2, 2, 3,  3, 3, 3, 3, 3, 3, 4, 4,  4, 4, 4, 4, 4, 4, 5, 5,  5, 5, 5, 5, 5, 6, 6, 6,
    6, 6, 6, 6, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,  7,
};
static const uint8_t code_size_table_v2[89] = {
    4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 5, 5, 5,  5, 5, 5, 5, 5, 5, 5, 5,  5, 5, 5, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,  7,
};

static const int8_t index_table2b[4] = {
   -1, 4,
   -1, 4,
};
static const int8_t index_table3b_v1[8] = {
   -1,-1, 2, 8,
   -1,-1, 2, 8,
};
static const int8_t index_table3b_v2[8] = {
   -1,-1, 2, 6,
   -1,-1, 2, 6,
};
static const int8_t index_table4b[16] = {
   -1,-1,-1,-1, 1, 2, 4, 6,
   -1,-1,-1,-1, 1, 2, 4, 6,
};
static const int8_t index_table5b_v1[32] = {
   -1,-1,-1,-1,-1,-1,-1,-1,     1, 2, 4, 6, 8,12,16,32,
   -1,-1,-1,-1,-1,-1,-1,-1,     1, 2, 4, 6, 8,12,16,32,
};
static const int8_t index_table5b_v2[32] = {
   -1,-1,-1,-1,-1,-1,-1,-1,     1, 1, 1, 2, 2, 4, 5, 6,
   -1,-1,-1,-1,-1,-1,-1,-1,     1, 1, 1, 2, 2, 4, 5, 6,
};
static const int8_t index_table6b_v1[64] = {
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,     1, 2, 4, 6, 8,10,12,14,    16,18,20,22,24,26,28,32,
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,     1, 2, 4, 6, 8,10,12,14,    16,18,20,22,24,26,28,32,
};
static const int8_t index_table6b_v2[64] = {
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,     1, 1, 1, 1, 1, 2, 2, 2,     2, 4, 4, 4, 5, 5, 6, 6,
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,     1, 1, 1, 1, 1, 2, 2, 2,     2, 4, 4, 4, 5, 5, 6, 6,
};
static const int8_t index_table7b_v1[128] = {
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,
    1, 2, 3, 4, 5, 6, 7, 8,     9,10,11,12,13,14,15,16,    17,18,19,20,21,22,23,24,    25,26,27,28,29,30,31,32,
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,
    1, 2, 3, 4, 5, 6, 7, 8,     9,10,11,12,13,14,15,16,    17,18,19,20,21,22,23,24,    25,26,27,28,29,30,31,32,
};
static const int8_t index_table7b_v2[128] = {
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,
    1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 2, 2, 2, 2, 2, 2,     2, 2, 4, 4, 4, 4, 4, 4,     5, 5, 5, 5, 6, 6, 6, 6,
   -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,
    1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 2, 2, 2, 2, 2, 2,     2, 2, 4, 4, 4, 4, 4, 4,     5, 5, 5, 5, 6, 6, 6, 6,
};

static const int8_t* index_tables_v1[8] = {
    NULL,
    NULL,
    index_table2b,
    index_table3b_v1,
    index_table4b,
    index_table5b_v1,
    index_table6b_v1,
    index_table7b_v1,
};
/* seems V2 doesn't actually use <4b, nor mirrored parts, even though they are defined */
static const int8_t* index_tables_v2[8] = {
    NULL,
    NULL,
    index_table2b,
    index_table3b_v2,
    index_table4b,
    index_table5b_v2,
    index_table6b_v2,
    index_table7b_v2,
};


#define MAX_CHANNELS 2
#define MAX_BLOCK_SIZE 0x2000
#define MAX_BLOCK_COUNT 0x10000  /* arbitrary max */

/* ************************** */

static int clamp_s32(int val, int min, int max) {
    if (val > max)
        return max;
    else if (val < min)
        return min;
    return val;
}

/* ************************** */

typedef enum { COMP, MCMP } imuse_type_t;

typedef struct {
    /* config */
    imuse_type_t type;
    int channels;

    size_t block_count;
    struct block_entry_t {
        uint32_t offset;    // absolute
        uint32_t size;      // block size
        uint32_t flags;     // block type
        uint32_t data;      // PCM bytes
    } *block_table;

    uint16_t adpcm_table[64 * 89];

    /* state */
    uint8_t block[MAX_BLOCK_SIZE];

    int current_block;
    short pbuf[MAX_BLOCK_SIZE / sizeof(short) * MAX_CHANNELS];
} imuse_codec_data;


static void free_imuse(void* priv_data) {
    imuse_codec_data* data = priv_data;
    if (!data) return;

    free(data->block_table);
    free(data);
}

static void reset_imuse(void* priv_data) {
    imuse_codec_data* data = priv_data;
    if (!data) return;

    data->current_block = 0;
}

imuse_codec_data* init_imuse_internal(int channels, int blocks) {
    if (channels > MAX_CHANNELS)
        return NULL;

    imuse_codec_data* data = calloc(1, sizeof(imuse_codec_data));
    if (!data) goto fail;

    data->channels = channels;

    data->block_count = blocks;
    if (data->block_count > MAX_BLOCK_COUNT) goto fail;

    data->block_table = calloc(data->block_count, sizeof(struct block_entry_t));
    if (!data->block_table) goto fail;

    // iMUSE pre-calculates main decode ops as a table, looks similar to standard IMA expand
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 89; j++) {
            int counter = 32;
            int value = 0;
            int step = step_table[j];
            while (counter > 0) {
                if (counter & i)
                    value += step;
                step >>= 1;
                counter >>= 1;
            }

            data->adpcm_table[i + j * 64] = value; /* non sequential: all 64 [0]s, [1]s ... [88]s */
        }
    }

    return data;
fail:
    free_imuse(data);
    return NULL;
}

void* init_imuse_mcomp(STREAMFILE* sf, int channels) {
    imuse_codec_data* data = NULL;

    /* read block table */
    if (is_id32be(0x00,sf, "COMP")) {
        int block_count = read_u32be(0x04,sf);
        // 08: base codec?
        // 0c: some size?
        
        data = init_imuse_internal(channels, block_count);
        if (!data) return NULL;

        uint32_t offset = 0x10;
        for (int i = 0; i < data->block_count; i++) {
            struct block_entry_t* entry = &data->block_table[i];

            entry->offset  = read_u32be(offset + 0x00, sf);
            entry->size    = read_u32be(offset + 0x04, sf);
            entry->flags   = read_u32be(offset + 0x08, sf);
            // 0x0c: null
            entry->data    = MAX_BLOCK_SIZE; // blocks decode into fixed size, that may include header

            if (entry->size > MAX_BLOCK_SIZE) {
                VGM_LOG("IMUSE: block size too big\n");
                goto fail;
            }

            // iMUSE blocks usually have VIMA but may contain mini-codecs (ex. The Dig)
            if (entry->flags != 0x0D && entry->flags != 0x0F) {
                VGM_LOG("IMUSE: unknown codec\n");
                goto fail;
            }

            offset += 0x10;
        }

        // detect type
        {
            uint32_t id = read_u32be(data->block_table[0].offset + 0x02, sf);
            if (id == get_id32be("iMUS")) { // [The Curse of Monkey Island (PC)]
                data->type = COMP;
            } else {
                goto fail; // no header [The Dig (PC)]
            }
        }
    }
    else if (is_id32be(0x00,sf, "MCMP")) {
        int block_count = read_u16be(0x04,sf);

        data = init_imuse_internal(channels, block_count);
        if (!data) return NULL;

        // pre-calculate for simpler logic
        uint32_t data_offset = 0x06 + data->block_count * 0x09;
        data_offset += 0x02 + read_u16be(data_offset + 0x00, sf); // mini text header

        uint32_t offset = 0x06;
        for (int i = 0; i < data->block_count; i++) {
            struct block_entry_t* entry = &data->block_table[i];

            entry->flags   = read_u8   (offset + 0x00, sf);
            entry->data    = read_u32be(offset + 0x01, sf);
            entry->size    = read_u32be(offset + 0x05, sf);
            entry->offset  = data_offset;
            // blocks of data and audio are separate

            if (entry->data > MAX_BLOCK_SIZE || entry->size > MAX_BLOCK_SIZE) {
                VGM_LOG("IMUSE: block size too big\n");
                goto fail;
            }

            // data or VIMA
            if (entry->flags != 0x00 && entry->flags != 0x01) {
                VGM_LOG("IMUSE: unknown codec\n");
                goto fail;
            }

            offset += 0x09;
            data_offset += entry->size;
        }

        data->type = MCMP; // with header [Grim Fandango (multi)]
    }
    else {
        goto fail;
    }

    return data;
fail:
    free_imuse(data);
    return NULL;
}

//TODO rename as init_vima_comp, init_vima_mcmp
void* init_imuse(STREAMFILE* sf, int channels) {
    off_t offset, data_offset;

    if (channels > MAX_CHANNELS)
        return NULL;

    imuse_codec_data* data = calloc(1, sizeof(imuse_codec_data));
    if (!data) goto fail;

    data->channels = channels;

    /* read index table */
    if (is_id32be(0x00,sf, "COMP")) {
        data->block_count = read_u32be(0x04,sf);
        if (data->block_count > MAX_BLOCK_COUNT) goto fail;
        /* 08: base codec? */
        /* 0c: some size? */

        data->block_table = calloc(data->block_count, sizeof(struct block_entry_t));
        if (!data->block_table) goto fail;

        offset = 0x10;
        for (int i = 0; i < data->block_count; i++) {
            struct block_entry_t* entry = &data->block_table[i];

            entry->offset  = read_u32be(offset + 0x00, sf);
            entry->size    = read_u32be(offset + 0x04, sf);
            entry->flags   = read_u32be(offset + 0x08, sf);
            /* 0x0c: null */
            entry->data    = MAX_BLOCK_SIZE;
            /* blocks decode into fixed size, that may include header */

            if (entry->size > MAX_BLOCK_SIZE) {
                VGM_LOG("IMUSE: block size too big\n");
                goto fail;
            }

            if (entry->flags != 0x0D && entry->flags != 0x0F) { /* VIMA */
                VGM_LOG("IMUSE: unknown codec\n");
                goto fail; /* others are bunch of mini-codecs (ex. The Dig) */
            }

            offset += 0x10;
        }

        /* detect type */
        {
            uint32_t id = read_u32be(data->block_table[0].offset + 0x02, sf);
            if (id == get_id32be("iMUS")) { /* [The Curse of Monkey Island (PC)] */
                data->type = COMP;
            } else {
                goto fail; /* no header [The Dig (PC)] */
            }
        }
    }
    else if (is_id32be(0x00,sf, "MCMP")) {
        data->block_count = read_u16be(0x04,sf);
        if (data->block_count > MAX_BLOCK_COUNT) goto fail;

        data->block_table = calloc(data->block_count, sizeof(struct block_entry_t));
        if (!data->block_table) goto fail;

        /* pre-calculate for simpler logic */
        data_offset = 0x06 + data->block_count * 0x09;
        data_offset += 0x02 + read_u16be(data_offset + 0x00, sf); /* mini text header */

        offset = 0x06;
        for (int i = 0; i < data->block_count; i++) {
            struct block_entry_t* entry = &data->block_table[i];

            entry->flags   = read_u8   (offset + 0x00, sf);
            entry->data    = read_u32be(offset + 0x01, sf);
            entry->size    = read_u32be(offset + 0x05, sf);
            entry->offset  = data_offset;
            /* blocks of data and audio are separate */

            if (entry->data > MAX_BLOCK_SIZE || entry->size > MAX_BLOCK_SIZE) {
                VGM_LOG("IMUSE: block size too big\n");
                goto fail;
            }

            if (entry->flags != 0x00 && entry->flags != 0x01) { /* data or VIMA */
                VGM_LOG("IMUSE: unknown codec\n");
                goto fail;
            }

            offset += 0x09;
            data_offset += entry->size;
        }

        data->type = MCMP; /* with header [Grim Fandango (multi)] */

        /* there are iMUS or RIFF headers but affect parser */
    }
    else {
        goto fail;
    }

    // iMUSE pre-calculates main decode ops as a table, looks similar to standard IMA expand
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 89; j++) {
            int counter = 32;
            int value = 0;
            int step = step_table[j];
            while (counter > 0) {
                if (counter & i)
                    value += step;
                step >>= 1;
                counter >>= 1;
            }

            data->adpcm_table[i + j * 64] = value; /* non sequential: all 64 [0]s, [1]s ... [88]s */
        }
    }

    reset_imuse(data);

    return data;

fail:
    free_imuse(data);
    return NULL;
}

/* **************************************** */

static int decode_vima_v1(short* samples, int chs, uint8_t* buf, size_t data_left, int block_num, uint16_t* adpcm_table) {
    int adpcm_history[MAX_CHANNELS] = {0};
    int adpcm_step_index[MAX_CHANNELS] = {0};

    int filled = 0;
    int pos = 0;

    /* read header (outside decode in original code) */
    {
        size_t copy_size;

        // decodes BLOCK_SIZE bytes (not samples), including copy_size if exists, but not first 16b
        // or ADPCM headers. ADPCM setup must be set to 0 if headers weren't read.
        copy_size = get_u16be(buf + pos);
        pos += 0x02;

        if (block_num == 0 && copy_size > 0) {
            /* iMUS header (always in first block) */
            pos += copy_size;
            data_left -= copy_size;
        }
        else if (copy_size > 0) {
            /* presumably PCM data (not seen) */
            for (int i = 0, j = pos; i < copy_size / sizeof(sample_t); i++, j += 2) {
                samples[i] = get_s16le(buf + j);
            }
            filled += copy_size / chs / sizeof(sample_t);

            pos += copy_size;
            data_left -= copy_size;

            VGM_LOG("IMUS: found PCM block %i\n", block_num);
        }
        else {
            /* ADPCM header (never in first block) */
            for (int i = 0; i < chs; i++) {
                adpcm_step_index[i] = get_u8   (buf + pos + 0x00);
              //adpcm_step[i]       = get_s32be(buf + pos + 0x01); /* same as step_table[step_index] */
                adpcm_history[i]    = get_s32be(buf + pos + 0x05);
                pos += 0x09;

                adpcm_step_index[i] = clamp_s32(adpcm_step_index[i], 0, 88); /* not originally */
            }
        }
    }

    bitstream_t is = {0};
    bm_setup(&is, buf, MAX_BLOCK_SIZE); // originally reads max 16 bits
    bm_skip(&is, pos * 8);

    /* decode ADPCM data after header (stereo layout: all samples from L, then all for R) */
    for (int ch = 0; ch < chs; ch++) {
        int first_sample = filled * chs + ch;
        int samples_left = data_left / sizeof(short);
        int samples_to_do;
        if (chs == 1) {
            samples_to_do = samples_left;
        }
        else {
            // L has +1 code for aligment at the end of block, must be read to reach R (code seems empty).
            // Not sure if COMI uses decoded bytes or decoded samples (returns samples_left / channels),
            // but the latter makes more sense.
            if (ch == 0)
                samples_to_do = (samples_left + 1) / chs;
            else
                samples_to_do = (samples_left + 0) / chs;
        }
        //;VGM_ASSERT((samples_left + 1) / 2 != (samples_left + 0) / 2, "IMUSE: sample diffs\n");

        int step_index = adpcm_step_index[ch];
        int sample = adpcm_history[ch];

        for (int i = 0, s = first_sample; i < samples_to_do; i++, s += chs) {
            int code_bits = code_size_table_v1[step_index];
            int sign_mask = (1 << (code_bits - 1));
            int data_mask = (sign_mask - 1); // done with a LUT in COMI

            int code = bm_read(&is, code_bits);
            int code_base = code & data_mask;

            {
                int delta = adpcm_table[(step_index * 64) + ((code_base << (7 - code_bits)))];
                delta += step_table[step_index] >> (code_bits - 1);
                if (code & sign_mask)
                    delta = -delta;

                sample += delta;
                sample = clamp16(sample);
            }

            samples[s] = sample;

            step_index += index_tables_v1[code_bits][code];
            step_index = clamp_s32(step_index, 0, 88);
        }
    }

    filled += data_left / sizeof(int16_t) / chs;
    return filled;
}

static int decode_vima_v2(short* samples, int chs, uint8_t* buf, size_t data_left, uint16_t* adpcm_table) {
    int adpcm_history[MAX_CHANNELS] = {0};
    int adpcm_step_index[MAX_CHANNELS] = {0};

    int filled = 0;
    int pos = 0;

    /* read ADPCM header */
    {
        for (int i = 0; i < chs; i++) {
            adpcm_step_index[i] = get_u8   (buf + pos + 0x00);
            adpcm_history[i]    = get_s16be(buf + pos + 0x01);
            pos += 0x03;

            // checked as < 0 and only for first index, means "stereo"
            if (adpcm_step_index[i] & 0x80) {
                adpcm_step_index[i] = (~adpcm_step_index[i]) & 0xFF;
                if (chs != 2) return 0;
            }

            // not originally done but in case of garbage data
            adpcm_step_index[i] = clamp_s32(adpcm_step_index[i], 0, 88);
        }
    }

    bitstream_t is = {0};
    bm_setup(&is, buf, MAX_BLOCK_SIZE); // originally reads max 16 bit w/ rolling buf
    bm_skip(&is, pos * 8);

    /* decode ADPCM data after header (stereo layout: all samples from L, then all for R) */
    for (int ch = 0; ch < chs; ch++) {
        int first_sample = filled * chs + ch;
        int samples_left = data_left / sizeof(short);
        int samples_to_do = samples_left / chs;

        int step_index = adpcm_step_index[ch];
        int sample = adpcm_history[ch];

        for (int i = 0, s = first_sample; i < samples_to_do; i++, s += chs) {
            int code_bits = code_size_table_v2[step_index];
            int sign_mask = (1 << (code_bits - 1));
            int data_mask = (sign_mask - 1);

            int code = bm_read(&is, code_bits);
            int code_base = code & data_mask;

            // all bits set means 'keyframe' = read next BE sample
            if (code_base == data_mask) {
                sample = (short)bm_read(&is, 16);
            }
            else {
                int delta = adpcm_table[(step_index * 64) + ((code_base << (7 - code_bits)))];
                if (code_base)
                    delta += step_table[step_index] >> (code_bits - 1);
                if (code & sign_mask)
                    delta = -delta;

                sample += delta;
                sample = clamp16(sample);
            }

            samples[s] = sample;

            step_index += index_tables_v2[code_bits][code];
            step_index = clamp_s32(step_index, 0, 88);
        }
    }

    filled += data_left / sizeof(int16_t) / chs;
    return filled;
}

static int decode_data_v2(short* samples, int chs, uint8_t* buf, size_t data_left, int block_num) {

    if (block_num == 0) {
        // iMUS header (always in first block, not shared with audio data unlike V1)
        return 0;
    }

    VGM_LOG("IMUSE: found PCM block %i\n", block_num);

    // presumably PCM data (not seen)
    for (int i = 0; i < data_left / sizeof(sample_t); i++) {
        samples[i] = get_s16le(buf);
        buf += 0x02;
    }

    return data_left / chs / sizeof(sample_t);
}

static int decode_block_v1(imuse_codec_data* data, uint8_t* block, size_t data_left) {
    int block_num = data->current_block;

    switch(data->block_table[block_num].flags) {
        case 0x0D:
        case 0x0F:
            return decode_vima_v1(data->pbuf, data->channels, block, data_left, block_num, data->adpcm_table);
        default:
            return -1;
    }
}

static int decode_block_v2(imuse_codec_data* data, uint8_t* block, size_t data_left) {
    int block_num = data->current_block;

    switch(data->block_table[block_num].flags) {
        case 0x00:
            return decode_data_v2(data->pbuf, data->channels, block, data_left, block_num);

        case 0x01:
            return decode_vima_v2(data->pbuf, data->channels, block, data_left, data->adpcm_table);

        default:
            return -1;
    }
}

// decodes a whole block into sample buffer, all at once due to L/R layout and VBR data
static int decode_block(sbuf_t* sbuf, imuse_codec_data* data) {
    size_t data_left  = data->block_table[data->current_block].data;

    int samples;
    switch(data->type) {
        case COMP:
            samples = decode_block_v1(data, data->block, data_left);
            break;

        case MCMP:
            samples = decode_block_v2(data, data->block, data_left);
            break;

        default:
            return -1;
    }

    // block done
    data->current_block++;

    return samples;
}

static bool read_block(STREAMFILE* sf, imuse_codec_data* data) {
    if (data->current_block >= data->block_count)
        return false;

    off_t offset = data->block_table[data->current_block].offset;
    size_t size  = data->block_table[data->current_block].size;

    read_streamfile(data->block, offset, size, sf);

    return true;
}

static bool decode_frame_imuse(VGMSTREAM* v) {
    imuse_codec_data* data = v->codec_data;
    STREAMFILE* sf = v->ch[0].streamfile;

    decode_state_t* ds = v->decode_state;

    bool ok = read_block(sf, data);
    if (!ok) return false;

    int samples = decode_block(&ds->sbuf, data);
    if (samples < 0) return false; //may be zero on header blocks

    sbuf_init_s16(&ds->sbuf, data->pbuf, samples, data->channels);
    ds->sbuf.filled = ds->sbuf.samples;

    return true;
}

static void seek_imuse(VGMSTREAM* v, int32_t num_sample) {
    imuse_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    if (!data) return;

    reset_imuse(data);

    //TODO: find closest block, set skip count
    ds->discard = num_sample;
}

const codec_info_t imuse_decoder = {
    .sample_type = SFMT_S16,
    .decode_frame = decode_frame_imuse,
    .free = free_imuse,
    .reset = reset_imuse,
    .seek = seek_imuse,
};
