#include "coding.h"


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

typedef struct {
    /*const*/ int16_t* samples;
    int filled;
    int channels;
    //todo may be more useful with filled/full? use 2 mark methods?
} sbuf_t;

/* copy, move and mark consumed samples */
static void sbuf_consume(sample_t** p_outbuf, int32_t* p_samples_to_do, sbuf_t* sbuf) {
    int samples_consume;

    samples_consume = *p_samples_to_do;
    if (samples_consume > sbuf->filled)
        samples_consume = sbuf->filled;

    /* memcpy is safe when filled/samples_copy is 0 (but must pass non-NULL bufs) */
    memcpy(*p_outbuf, sbuf->samples, samples_consume * sbuf->channels * sizeof(int16_t));

    sbuf->samples += samples_consume * sbuf->channels;
    sbuf->filled -= samples_consume;

    *p_outbuf += samples_consume * sbuf->channels;
    *p_samples_to_do -= samples_consume;
}

static int clamp_s32(int val, int min, int max) {
    if (val > max)
        return max;
    else if (val < min)
        return min;
    return val;
}

/* ************************** */

typedef enum { COMP, MCMP } imuse_type_t;
struct imuse_codec_data {
    /* config */
    imuse_type_t type;
    int channels;

    size_t block_count;
    struct block_entry_t {
        uint32_t offset; /* from file start */
        uint32_t size;
        uint32_t flags;
        uint32_t data;
    } *block_table;

    uint16_t adpcm_table[64 * 89];

    /* state */
    sbuf_t sbuf;
    int current_block;
    int16_t samples[MAX_BLOCK_SIZE / sizeof(int16_t) * MAX_CHANNELS];
};


imuse_codec_data* init_imuse(STREAMFILE* sf, int channels) {
    int i, j;
    off_t offset, data_offset;
    imuse_codec_data* data = NULL;

    if (channels > MAX_CHANNELS)
        goto fail;

    data = calloc(1, sizeof(struct imuse_codec_data));
    if (!data) goto fail;

    data->channels = channels;

    /* read index table */
    if (read_u32be(0x00,sf) == 0x434F4D50) { /* "COMP" */
        data->block_count = read_u32be(0x04,sf);
        if (data->block_count > MAX_BLOCK_COUNT) goto fail;
        /* 08: base codec? */
        /* 0c: some size? */

        data->block_table = calloc(data->block_count, sizeof(struct block_entry_t));
        if (!data->block_table) goto fail;

        offset = 0x10;
        for (i = 0; i < data->block_count; i++) {
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
            if (id == 0x694D5553) { /* "iMUS" header [The Curse of Monkey Island (PC)] */
                data->type = COMP;
            } else {
                goto fail; /* no header [The Dig (PC)] */
            }
        }
    }
    else if (read_u32be(0x00,sf) == 0x4D434D50) { /* "MCMP" */
        data->block_count = read_u16be(0x04,sf);
        if (data->block_count > MAX_BLOCK_COUNT) goto fail;

        data->block_table = calloc(data->block_count, sizeof(struct block_entry_t));
        if (!data->block_table) goto fail;

        /* pre-calculate for simpler logic */
        data_offset = 0x06 + data->block_count * 0x09;
        data_offset += 0x02 + read_u16be(data_offset + 0x00, sf); /* mini text header */

        offset = 0x06;
        for (i = 0; i < data->block_count; i++) {
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

    /* iMUSE pre-calculates main decode ops as a table, looks similar to standard IMA expand */
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 89; j++) {
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

static void decode_vima1(sbuf_t* sbuf, uint8_t* buf, size_t data_left, int block_num, uint16_t* adpcm_table) {
    int ch, i, j, s;
    int bitpos;
    int adpcm_history[MAX_CHANNELS] = {0};
    int adpcm_step_index[MAX_CHANNELS] = {0};
    int chs = sbuf->channels;

    /* read header (outside decode in original code) */
    {
        int pos = 0;
        size_t copy_size;

        /* decodes BLOCK_SIZE bytes (not samples), including copy_size if exists, but not first 16b
         * or ADPCM headers. ADPCM setup must be set to 0 if headers weren't read. */
        copy_size = get_u16be(buf + pos);
        pos += 0x02;

        if (block_num == 0 && copy_size > 0) {
            /* iMUS header (always in first block) */
            pos += copy_size;
            data_left -= copy_size;
        }
        else if (copy_size > 0) {
            /* presumably PCM data (not seen) */
            for (i = 0, j = pos; i < copy_size / sizeof(sample_t); i++, j += 2) {
                sbuf->samples[i] = get_s16le(buf + j);
            }
            sbuf->filled += copy_size / chs / sizeof(sample_t);

            pos += copy_size;
            data_left -= copy_size;

            VGM_LOG("IMUS: found PCM block %i\n", block_num);
        }
        else {
            /* ADPCM header (never in first block) */
            for (i = 0; i < chs; i++) {
                adpcm_step_index[i] = get_u8   (buf + pos + 0x00);
              //adpcm_step[i]       = get_s32be(buf + pos + 0x01); /* same as step_table[step_index] */
                adpcm_history[i]    = get_s32be(buf + pos + 0x05);
                pos += 0x09;

                adpcm_step_index[i] = clamp_s32(adpcm_step_index[i], 0, 88); /* not originally */
            }
        }

        bitpos = pos * 8;
    }


    /* decode ADPCM data after header
     * (stereo layout: all samples from L, then all for R) */
    for (ch = 0; ch < chs; ch++) {
        int sample, step_index;
        int samples_to_do;
        int samples_left = data_left / sizeof(int16_t);
        int first_sample = sbuf->filled * chs + ch;

        if (chs == 1) {
            samples_to_do = samples_left;
        } else {
            /* L has +1 code for aligment in first block, must be read to reach R (code seems empty).
             * Not sure if COMI uses decoded bytes or decoded samples (returns samples_left / channels)
             * though but the latter makes more sense. */
            if (ch == 0)
                samples_to_do = (samples_left + 1) / chs;
            else
                samples_to_do = (samples_left + 0) / chs;
        }

        //;VGM_ASSERT((samples_left + 1) / 2 != (samples_left + 0) / 2, "IMUSE: sample diffs\n");

        step_index = adpcm_step_index[ch];
        sample = adpcm_history[ch];

        for (i = 0, s = first_sample; i < samples_to_do; i++, s += chs) {
            int code_size, code, sign_mask, data_mask, delta;

            if (bitpos >= MAX_BLOCK_SIZE * 8) {
                VGM_LOG("IMUSE: wrong bit offset\n");
                break;
            }

            code_size = code_size_table_v1[step_index];

            /* get bit thing from COMI (reads closest 16b then masks + shifts as needed), BE layout */
            code = get_u16be(buf + (bitpos >> 3)); //ok
            code = (code << (bitpos & 7)) & 0xFFFF;
            code = code >> (16 - code_size);
            bitpos += code_size;

            sign_mask = (1 << (code_size - 1));
            data_mask = sign_mask - 1; /* done with a LUT in COMI */

            delta  = adpcm_table[(step_index * 64) + (((code & data_mask) << (7 - code_size)))];
            delta += step_table[step_index] >> (code_size - 1);
            if (code & sign_mask)
                delta = -delta;

            sample += delta;
            sample = clamp16(sample);
            sbuf->samples[s] = sample;

            step_index += index_tables_v1[code_size][code];
            step_index = clamp_s32(step_index, 0, 88);
        }
    }

    sbuf->filled += data_left / sizeof(int16_t) / chs;
}

static int decode_block1(imuse_codec_data* data, uint8_t* block, size_t data_left) {
    int block_num = data->current_block;

    switch(data->block_table[block_num].flags) {
        case 0x0D:
        case 0x0F:
            decode_vima1(&data->sbuf, block, data_left, block_num, data->adpcm_table);
            break;
        default:
            return 0;
    }
    return 1;
}

static void decode_data2(sbuf_t* sbuf, uint8_t* buf, size_t data_left, int block_num) {
    int i, j;
    int channels = sbuf->channels;

    if (block_num == 0) {
        /* iMUS header (always in first block, not shared with audio data unlike V1) */
        sbuf->filled = 0;
    }
    else {
        /* presumably PCM data (not seen) */
        for (i = 0, j = 0; i < data_left / sizeof(sample_t); i++, j += 2) {
            sbuf->samples[i] = get_s16le(buf + j);
        }
        sbuf->filled += data_left / channels / sizeof(sample_t);

        VGM_LOG("IMUS: found PCM block %i\n", block_num);
    }
}

static void decode_vima2(sbuf_t* sbuf, uint8_t* buf, size_t data_left, uint16_t* adpcm_table) {
    int ch, i, s;
    int bitpos;
    int adpcm_history[MAX_CHANNELS] = {0};
    int adpcm_step_index[MAX_CHANNELS] = {0};
    int chs = sbuf->channels;
    uint16_t word;
    int pos = 0;


    /* read ADPCM header */
    {

        for (i = 0; i < chs; i++) {
            adpcm_step_index[i] = get_u8   (buf + pos + 0x00);
            adpcm_history[i]    = get_s16be(buf + pos + 0x01);
            pos += 0x03;

            /* checked as < 0 and only for first index, means "stereo" */
            if (adpcm_step_index[i] & 0x80) {
                adpcm_step_index[i] = (~adpcm_step_index[i]) & 0xFF;
                if (chs != 2) return;
            }

            /* not originally done but in case of garbage data */
            adpcm_step_index[i] = clamp_s32(adpcm_step_index[i], 0, 88);
        }

    }

    bitpos = 0;
    word = get_u16be(buf + pos); /* originally with a rolling buf, use index to validate overflow */
    pos += 0x02;

    /* decode ADPCM data after header
     * (stereo layout: all samples from L, then all for R) */
    for (ch = 0; ch < chs; ch++) {
        int sample, step_index;
        int samples_to_do;
        int samples_left = data_left / sizeof(int16_t);
        int first_sample = sbuf->filled * chs + ch;

        samples_to_do = samples_left / chs;

        step_index = adpcm_step_index[ch];
        sample = adpcm_history[ch];

        for (i = 0, s = first_sample; i < samples_to_do; i++, s += chs) {
            int code_size, code, sign_mask, data_mask, delta;

            if (pos >= MAX_BLOCK_SIZE) {
                VGM_LOG("IMUSE: wrong pos offset\n");
                break;
            }

            code_size = code_size_table_v2[step_index];
            sign_mask = (1 << (code_size - 1));
            data_mask = (sign_mask - 1);

            /* get bit thing, masks current code and moves 'upwards' word after reading 8 bits */
            bitpos += code_size;
            code = (word >> (16 - bitpos)) & (sign_mask | data_mask);
            if (bitpos > 7) {
                word = (word << 8) | buf[pos++];
                bitpos -= 8;
            }

            /* clean sign stuff for next tests */
            if (code & sign_mask)
                code ^= sign_mask;
            else
                sign_mask = 0;

            /* all bits set mean 'keyframe' = read next sample */
            if (code == data_mask) {
                sample  = (int16_t)(word << bitpos);
                word = (word << 8) | buf[pos++];
                sample |= (word >> (8 - bitpos)) & 0xFF;
                word = (word << 8) | buf[pos++];
            }
            else {
                delta = adpcm_table[(step_index * 64) + ((code << (7 - code_size)))];
                if (code)
                    delta += step_table[step_index] >> (code_size - 1);
                if (sign_mask)
                    delta = -delta;

                sample += delta;
                sample = clamp16(sample);
            }

            sbuf->samples[s] = sample;

            step_index += index_tables_v2[code_size][code];
            step_index = clamp_s32(step_index, 0, 88);
        }
    }

    sbuf->filled += data_left / sizeof(int16_t) / chs;
}

static int decode_block2(imuse_codec_data* data, uint8_t* block, size_t data_left) {
    int block_num = data->current_block;

    switch(data->block_table[block_num].flags) {
        case 0x00:
            decode_data2(&data->sbuf, block, data_left, block_num);
            break;

        case 0x01:
            decode_vima2(&data->sbuf, block, data_left, data->adpcm_table);
            break;
        default:
            return 0;
    }
    return 1;
}


/* decodes a whole block into sample buffer, all at once due to L/R layout and VBR data */
static int decode_block(STREAMFILE* sf, imuse_codec_data* data) {
    int ok;
    uint8_t block[MAX_BLOCK_SIZE];
    size_t data_left;

    data->sbuf.samples = data->samples;
    data->sbuf.channels = data->channels;

    if (data->current_block >= data->block_count) {
        return 0;
    }

    /* read block */
    {
        off_t offset = data->block_table[data->current_block].offset;
        size_t size  = data->block_table[data->current_block].size;

        read_streamfile(block, offset, size, sf);

        data_left    = data->block_table[data->current_block].data;
    }

    switch(data->type) {
        case COMP:
            ok = decode_block1(data, block, data_left);
            break;

        case MCMP:
            ok = decode_block2(data, block, data_left);
            break;

        default:
            return 0;
    }

    /* block fully read */
    data->current_block++;

    return ok;
}


void decode_imuse(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do) {
    imuse_codec_data* data = vgmstream->codec_data;
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int ok;


    while (samples_to_do > 0) {
        sbuf_t* sbuf = &data->sbuf;

        if (sbuf->filled == 0) {
            ok = decode_block(sf, data);
            if (!ok) goto fail;
        }

        sbuf_consume(&outbuf, &samples_to_do, sbuf);
    }

    return;
fail:
    //todo fill silence
    return;
}


void free_imuse(imuse_codec_data* data) {
    if (!data) return;

    free(data->block_table);
    free(data);
}

void seek_imuse(imuse_codec_data* data, int32_t num_sample) {
    if (!data) return;

    //todo find closest block, set skip count

    reset_imuse(data);
}

void reset_imuse(imuse_codec_data* data) {
    if (!data) return;

    data->current_block = 0;
    data->sbuf.filled = 0;
}
