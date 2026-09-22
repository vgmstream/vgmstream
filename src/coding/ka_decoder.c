#include "coding.h"
#include "../base/codec_info.h"
#include "../base/decode_state.h"
#include "../util/bitstream_msb.h"


typedef struct {
    uint8_t symbol;
    uint8_t bits;
} ka_code_t;

typedef struct {
    ka_code_t (*codes)[0x100];
    uint8_t* buf;
    int16_t* pbuf;
    int bits, scale, leak;
    int contexts, code_bytes;
    uint32_t block_samples;
    uint32_t step, step_min, step_max;
    const int* multipliers;
    int32_t num_samples;
    int32_t remaining;
} ka_codec_data;

/* ADPCM magnitude multipliers (fixed point, divided by 64). */
static const int step_multipliers_2[2] = {51, 102};
static const int step_multipliers_3[4] = {58, 58, 80, 112};
static const int step_multipliers_4[8] = {58, 58, 58, 58, 77, 102, 128, 154};
static const int step_multipliers_6[32] = {
    58, 58, 58, 58, 58, 58, 58, 58,
    58, 58, 58, 58, 58, 58, 58, 58,
    70, 83, 96, 109, 122, 134, 147, 160,
    173, 186, 198, 211, 224, 237, 250, 262,
};

static bool read_tables(ka_codec_data* data, STREAMFILE* sf, off_t offset) {
    for (int context = 0; context < data->contexts; context++) {
        uint8_t row[0x80], lengths[0x100] = {0};
        int counts[9] = {0}, next[9] = {0};
        int slots = 1, code = 0;
        int length_sum = 0, last_symbol = 0;

        if (read_streamfile(row, offset, data->code_bytes, sf) != data->code_bytes)
            return false;
        offset += data->code_bytes;

        for (int symbol = 0; symbol < data->contexts && symbol < data->code_bytes * 2; symbol++) {
            int bits = (row[symbol / 2] >> (4 * (symbol % 2))) & 0x0f;
            if (bits > 8)
                return false;
            lengths[symbol] = bits;
            if (bits) {
                counts[bits]++;
                length_sum += bits;
                last_symbol = symbol;
            }
        }

        /* A single one-bit symbol is repeated without consuming input. */
        if (length_sum == 1) {
            for (int i = 0; i < 0x100; i++) {
                data->codes[context][i].symbol = last_symbol;
                data->codes[context][i].bits = 0;
            }
            continue;
        }

        /* Unused contexts/prefixes stay invalid. */
        if (!length_sum)
            continue;
        for (int bits = 1; bits <= 8; bits++) {
            slots = slots * 2 - counts[bits];
            if (slots < 0)
                return false;
            code = (code + counts[bits - 1]) * 2;
            next[bits] = code;
        }
        for (int symbol = 0; symbol < data->contexts; symbol++) {
            int bits = lengths[symbol];
            if (!bits)
                continue;
            int first = next[bits]++ << (8 - bits);
            int count = 1 << (8 - bits);
            if (first + count > 0x100)
                return false;
            for (int i = first; i < first + count; i++) {
                data->codes[context][i].symbol = symbol;
                data->codes[context][i].bits = bits;
            }
        }
    }
    return true;
}

static void reset_ka(void* priv_data) {
    ka_codec_data* data = priv_data;
    if (!data) return;
    data->step = data->step_min;
    data->remaining = data->num_samples;
}

static void free_ka(void* priv_data) {
    ka_codec_data* data = priv_data;
    if (!data) return;
    free(data->codes);
    free(data->buf);
    free(data->pbuf);
    free(data);
}

/* Knowledge Adventure ADPCM */
void* init_ka(STREAMFILE* sf, off_t table_offset, int32_t num_samples) {
    uint32_t block_size;
    int version;
    ka_codec_data* data = calloc(1, sizeof(ka_codec_data));
    if (!data) return NULL;

    version = read_u8(0x0e, sf);
    data->bits = read_u8(0x11, sf);
    data->scale = read_u8(0x15, sf);
    data->leak = read_u16le(0x16, sf);
    data->contexts = read_u16le(0x18, sf);
    data->code_bytes = read_u16le(0x1a, sf);
    block_size = read_u32le(0x1c, sf);

    switch (data->bits) {
        case 2: data->multipliers = step_multipliers_2; break;
        case 3: data->multipliers = step_multipliers_3; break;
        case 4: data->multipliers = step_multipliers_4; break;
        case 6: data->multipliers = step_multipliers_6; break;
        default: goto fail;
    }
    /* The original Huffman context offsets occupy one byte in units of three. */
    if (data->contexts < 1 || data->contexts > 0x55 ||
            data->code_bytes < 1 || data->code_bytes > (data->contexts + 1) / 2)
        goto fail;
    if (data->contexts * data->code_bytes > read_u16le(0x34, sf))
        goto fail;

    data->block_samples = block_size / 2;
    if (data->block_samples > num_samples)
        data->block_samples = num_samples;
    /* Keep the bitreader's signed bit positions representable. */
    if (data->block_samples < 1 || data->block_samples > INT32_MAX / 8 - 1)
        goto fail;
    data->codes = malloc(data->contexts * sizeof(*data->codes));
    data->buf = malloc(data->block_samples + 1);
    data->pbuf = malloc(data->block_samples * sizeof(*data->pbuf));
    if (!data->codes || !data->buf || !data->pbuf)
        goto fail;
    memset(data->codes, 0xff, data->contexts * sizeof(*data->codes));

    if (!read_tables(data, sf, table_offset))
        goto fail;
    data->step_min = (1u << data->bits) * data->scale;
    data->step_max = (uint32_t)data->scale << (version == 1 ? 0x10 : 0x11);
    data->num_samples = num_samples;
    reset_ka(data);
    return data;
fail:
    free_ka(data);
    return NULL;
}

static bool decode_frame_ka(VGMSTREAM* v) {
    ka_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    VGMSTREAMCHANNEL* ch = &v->ch[0];
    STREAMFILE* sf = ch->streamfile;
    size_t file_size = get_streamfile_size(sf);
    uint32_t bytes, samples;
    bitstream_t bs;
    int32_t predictor;
    uint32_t step = data->step;
    int context = 0;

    if (ch->offset < 0 || ch->offset > file_size || file_size - ch->offset < 0x08)
        return false;
    bytes = read_u32le(ch->offset, sf);
    samples = read_u32le(ch->offset + 0x04, sf);
    if (samples < 1 || samples > data->block_samples || samples > data->remaining)
        return false;
    if (bytes < 2 || bytes > data->block_samples + 1 || bytes > file_size - ch->offset - 0x08)
        return false;
    if (read_streamfile(data->buf, ch->offset + 0x08, bytes, sf) != bytes)
        return false;

    /* Each block resets its predictor and Huffman context, but carries the step. */
    predictor = get_s16le(data->buf) * data->scale;
    data->pbuf[0] = predictor;
    if (step < data->step_min) step = data->step_min;
    if (step > data->step_max) step = data->step_max;
    bm_setup(&bs, data->buf + 2, bytes - 2);

    for (int i = 1; i < samples; i++) {
        int left = (bytes - 2) * 8 - bm_pos(&bs);
        int peek_bits = left < 8 ? left : 8;
        bitstream_t peek = bs;
        uint32_t slot;
        ka_code_t entry;
        int magnitude, delta;

        /* A short final lookup is padded only for peeking, */
        if (!bm_get(&peek, peek_bits, &slot))
            return false;
        slot <<= 8 - peek_bits;
        entry = data->codes[context][slot];
        if (entry.bits == 0xff || entry.symbol >= (1 << data->bits) || !bm_skip(&bs, entry.bits))
            return false;
        context = entry.symbol;

        magnitude = entry.symbol >> 1;
        delta = ((1u << (data->bits - 1)) + (magnitude + 1) * step) >> data->bits;
        predictor = (int32_t)((uint32_t)predictor * data->leak + 0x40) >> 7;
        if (entry.symbol & 1) {
            predictor -= delta;
            if (predictor < -0x8000) predictor = -0x8000;
        }
        else {
            predictor += delta;
            if (predictor > 0x7FFF) predictor = 0x7FFF;
        }
        data->pbuf[i] = predictor;

        step = (step * data->multipliers[magnitude] + 0x20) >> 6;
        if (step < data->step_min) step = data->step_min;
        if (step > data->step_max) step = data->step_max;
    }

    if ((bytes - 2) * 8 - bm_pos(&bs) >= 8)
        return false;

    data->step = step;
    data->remaining -= samples;
    ch->offset += 0x08 + bytes;
    sbuf_init_s16(&ds->sbuf, data->pbuf, samples, 1);
    ds->sbuf.filled = samples;
    return true;
}

static void seek_ka(VGMSTREAM* v, int32_t num_sample) {
    ka_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    reset_ka(data);

    ds->discard = num_sample;
    v->ch[0].offset = v->ch[0].channel_start_offset;
    if (v->loop_ch)
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
}

const codec_info_t ka_decoder = {
    .sample_type = SFMT_S16,
    .decode_frame = decode_frame_ka,
    .free = free_ka,
    .reset = reset_ka,
    .seek = seek_ka,
};
