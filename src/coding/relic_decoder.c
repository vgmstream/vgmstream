#include <math.h>
#include "coding.h"

/* Relic Codec decoder, a fairly simple mono-interleave DCT-based codec.
 *
 * Decompiled from Relic's dec.exe with some info from Homeworld source code .h/lib
 * files (released around 2003 through Relic Dev Network), accurate with minor +-1
 * samples due to double<>float ops or maybe original compiler (Intel's) diffs.
 * 
 * TODO: clean API, improve validations (can segfault on bad data) and naming
 */

/* mixfft.c */
extern void fft(int n, float *xRe, float *xIm, float *yRe, float *yIm);

static relic_codec_data* init_codec(int channels, int bitrate, int codec_rate);
static int decode_frame_next(VGMSTREAMCHANNEL* stream, relic_codec_data* data);
static void copy_samples(relic_codec_data* data, sample_t* outbuf, int32_t samples_to_get);
static void reset_codec(relic_codec_data* data);

#define RELIC_MAX_CHANNELS  2
#define RELIC_MAX_SCALES  6
#define RELIC_BASE_SCALE  10.0f
#define RELIC_FREQUENCY_MASKING_FACTOR  1.0f
#define RELIC_CRITICAL_BAND_COUNT  27
#define RELIC_PI  3.14159265358979323846f
#define RELIC_SIZE_LOW  128
#define RELIC_SIZE_MID  256
#define RELIC_SIZE_HIGH 512
#define RELIC_MAX_SIZE  RELIC_SIZE_HIGH
#define RELIC_MAX_FREQ  (RELIC_MAX_SIZE / 2)
#define RELIC_MAX_FFT   (RELIC_MAX_SIZE / 4)
#define	RELIC_BITRATE_22    256
#define	RELIC_BITRATE_44    512
#define	RELIC_BITRATE_88    1024
#define	RELIC_BITRATE_176   2048
#define RELIC_MAX_FRAME_SIZE  ((RELIC_BITRATE_176 / 8) + 0x04) /* extra 0x04 for the bitreader */


struct relic_codec_data {
    /* decoder info */
    int channels;
    int frame_size;
    int wave_size;
    int freq_size;
    int dct_mode;
    int samples_mode;
    /* decoder init state */
    float scales[RELIC_MAX_SCALES]; /* quantization scales */
    float dct[RELIC_MAX_SIZE];
    float window[RELIC_MAX_SIZE];
    /* decoder frame state */
    uint8_t exponents[RELIC_MAX_CHANNELS][RELIC_MAX_FREQ]; /* quantization/scale indexes */
    float freq1[RELIC_MAX_FREQ]; /* dequantized spectrum */
    float freq2[RELIC_MAX_FREQ];
    float wave_cur[RELIC_MAX_CHANNELS][RELIC_MAX_SIZE]; /* current frame samples */
    float wave_prv[RELIC_MAX_CHANNELS][RELIC_MAX_SIZE]; /* previous frame samples */

    /* sample state */
    int32_t samples_discard;
    int32_t samples_consumed;
    int32_t samples_filled;
};

/* ************************************* */


relic_codec_data* init_relic(int channels, int bitrate, int codec_rate) {
    return init_codec(channels, bitrate, codec_rate);
}

void decode_relic(VGMSTREAMCHANNEL* stream, relic_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {

    while (samples_to_do > 0) {

        if (data->samples_consumed < data->samples_filled) {
            /* consume samples */
            int samples_to_get = (data->samples_filled - data->samples_consumed);

            if (data->samples_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_discard)
                    samples_to_get = data->samples_discard;
                data->samples_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do)
                    samples_to_get = samples_to_do;

                copy_samples(data, outbuf, samples_to_get);

                samples_to_do -= samples_to_get;
                outbuf += samples_to_get * data->channels;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
        }
        else {
            int ok = decode_frame_next(stream, data);
            if (!ok) goto decode_fail;
        }
    }
    return;

decode_fail:
    /* on error just put some 0 samples */
    VGM_LOG("RELIC: decode fail, missing %i samples\n", samples_to_do);
    memset(outbuf, 0, samples_to_do * data->channels * sizeof(sample));
}

void reset_relic(relic_codec_data* data) {
    if (!data) return;

    reset_codec(data);
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_discard = 0;
}

void seek_relic(relic_codec_data* data, int32_t num_sample) {
    if (!data) return;

    reset_relic(data);
    data->samples_discard = num_sample;
}

void free_relic(relic_codec_data* data) {
    if (!data) return;

    free(data);
}

/* ***************************************** */

static const int16_t critical_band_data[RELIC_CRITICAL_BAND_COUNT] = { 
    0, 1, 2, 3, 4, 5, 6, 7,
    9, 11, 13, 15, 17, 20, 23, 27,
    31, 37, 43, 51, 62, 74, 89, 110,
    139, 180, 256
};

static void init_dct(float *dct, int dct_size) {
    int i;
    int dct_quarter = dct_size >> 2;

    for (i = 0; i < dct_quarter; i++) {
        double temp = ((float)i + 0.125f) * (RELIC_PI * 2.0f) * (1.0f / (float)dct_size);
        dct[i] = sin(temp);
        dct[dct_quarter + i] = cos(temp);
    }
}

static int apply_idct(const float *freq, float *wave, const float *dct, int dct_size) {
    int i;
    float factor;
    float out_re[RELIC_MAX_FFT];
    float out_im[RELIC_MAX_FFT];
    float in_re[RELIC_MAX_FFT];
    float in_im[RELIC_MAX_FFT];
    float wave_tmp[RELIC_MAX_SIZE];
    int dct_half = dct_size >> 1;
    int dct_quarter = dct_size >> 2;
    int dct_3quarter = 3 * (dct_size >> 2);

    /* prerotation? */
    for (i = 0; i < dct_quarter; i++) {
        float coef1 = freq[2 * i] * 0.5f;
        float coef2 = freq[dct_half - 1 - 2 * i] * 0.5f;
        in_re[i] = coef1 * dct[dct_quarter + i] + coef2 * dct[i];
        in_im[i] = -coef1 * dct[i] + coef2 * dct[dct_quarter + i];
    }

    /* main FFT */
    fft(dct_quarter, in_re, in_im, out_re, out_im);

    /* postrotation, window and reorder? */
    factor = 8.0 / sqrt(dct_size);
    for (i = 0; i < dct_quarter; i++) {
        float out_re_i = out_re[i];
        out_re[i] = (out_re[i] * dct[dct_quarter + i] + out_im[i] * dct[i]) * factor;
        out_im[i] = (-out_re_i * dct[i] + out_im[i] * dct[dct_quarter + i]) * factor;
        wave_tmp[i * 2] = out_re[i];
        wave_tmp[i * 2 + dct_half] = out_im[i];
    }
    for (i = 1; i < dct_size; i += 2) {
        wave_tmp[i] = -wave_tmp[dct_size - 1 - i];
    }

    /* wave mix thing? */
    for (i = 0; i < dct_3quarter; i++) {
        wave[i] = wave_tmp[dct_quarter + i];
    }
    for (i = dct_3quarter; i < dct_size; i++) {
        wave[i] = -wave_tmp[i - dct_3quarter];
    }
    return 0;
}

static void decode_frame(const float *freq1, const float *freq2, float *wave_cur, float *wave_prv, const float *dct, const float *window, int dct_size) {
    int i;
    float wave_tmp[RELIC_MAX_SIZE];
    int dct_half = dct_size >> 1;

    /* copy for first half(?) */
    memcpy(wave_cur, wave_prv, RELIC_MAX_SIZE * sizeof(float));

    /* transform frequency domain to time domain with DCT/FFT */
    apply_idct(freq1, wave_tmp, dct, dct_size);
    apply_idct(freq2, wave_prv, dct, dct_size);

    /* overlap and apply window function to filter this block's beginning */
    for (i = 0; i < dct_half; i++) {
        wave_cur[dct_half + i] = wave_tmp[i] * window[i] + wave_cur[dct_half + i] * window[dct_half + i];
        wave_prv[i]            = wave_prv[i] * window[i] + wave_tmp[dct_half + i] * window[dct_half + i];
    }
}

static void init_window(float *window, int dct_size) {
    int i;

    for (i = 0; i < dct_size; i++) {
        window[i] = sin((float)i * (RELIC_PI / dct_size));
    }
}

static void decode_frame_base(const float *freq1, const float *freq2, float *wave_cur, float *wave_prv, const float *dct, const float *window, int dct_mode, int samples_mode) {
    int i;
    float wave_tmp[RELIC_MAX_SIZE];

    /* dec_relic only uses 512/512 mode, source references 256/256 (effects only?) too */

    if (samples_mode == RELIC_SIZE_LOW) {
        {
            /* 128 DCT to 128 samples */
            decode_frame(freq1, freq2, wave_cur, wave_prv, dct, window, RELIC_SIZE_LOW);
        }
    }
    else if (samples_mode == RELIC_SIZE_MID) {
        if (dct_mode == RELIC_SIZE_LOW) { 
            /* 128 DCT to 256 samples (repeat sample x2) */
            decode_frame(freq1, freq2, wave_tmp, wave_prv, dct, window, RELIC_SIZE_LOW);
            for (i = 0; i < 256 - 1; i += 2) {
                wave_cur[i + 0] = wave_tmp[i >> 1];
                wave_cur[i + 1] = wave_tmp[i >> 1];
            }
        }
        else {
            /* 256 DCT to 256 samples */
            decode_frame(freq1, freq2, wave_cur, wave_prv, dct, window, RELIC_SIZE_MID);
        }
    }
    else if (samples_mode == RELIC_SIZE_HIGH) {
        if (dct_mode == RELIC_SIZE_LOW) {
            /* 128 DCT to 512 samples (repeat sample x4) */
            decode_frame(freq1, freq2, wave_tmp, wave_prv, dct, window, RELIC_SIZE_LOW);
            for (i = 0; i < 512 - 1; i += 4) {
                wave_cur[i + 0] = wave_tmp[i >> 2];
                wave_cur[i + 1] = wave_tmp[i >> 2];
                wave_cur[i + 2] = wave_tmp[i >> 2];
                wave_cur[i + 3] = wave_tmp[i >> 2];
            }
        }
        else if (dct_mode == RELIC_SIZE_MID) {
            /* 256 DCT to 512 samples (repeat sample x2) */
            decode_frame(freq1, freq2, wave_tmp, wave_prv, dct, window, RELIC_SIZE_MID);
            for (i = 0; i < 512 - 1; i += 2) {
                wave_cur[i + 0] = wave_tmp[i >> 1];
                wave_cur[i + 1] = wave_tmp[i >> 1];
            }
        }
        else {
            /* 512 DCT to 512 samples */
            decode_frame(freq1, freq2, wave_cur, wave_prv, dct, window, RELIC_SIZE_HIGH);
        }
    }
}


/* reads 32b max, packed in LSB order per byte (like Vorbis), ex. 
 * with 0x45 6A=01000101 01101010 could read 4b=0101, 6b=100100, 3b=010 ...
 * assumes buf has enough extra bits to read 32b (size +0x04) */
static uint32_t read_ubits(uint8_t bits, uint32_t offset, uint8_t *buf) {
    uint32_t shift, mask, pos, val;

    shift = offset - 8 * (offset / 8);
    mask = (1 << bits) - 1;
    pos = offset / 8;
    val = (buf[pos+0]) | (buf[pos+1]<<8) | (buf[pos+2]<<16) | (buf[pos+3]<<24);
    return (val >> shift) & mask;
}
static int read_sbits(uint8_t bits, uint32_t offset, uint8_t *buf) {
    uint32_t val = read_ubits(bits, offset, buf);
    int outval;
    if (val >> (bits - 1) == 1) { /* upper bit = sign */
        uint32_t mask = (1 << (bits - 1)) - 1;
        outval = (int)(val & mask);
        outval = -outval;
    }
    else {
        outval = (int)val;
    }
    return outval;
}

static void init_dequantization(float* scales) {
    int i;

    scales[0] = RELIC_BASE_SCALE;
    for (i = 1; i < RELIC_MAX_SCALES; i++) {
        scales[i] = scales[i - 1] * scales[0];
    }
    for (i = 0; i < RELIC_MAX_SCALES; i++) {
        scales[i] = RELIC_FREQUENCY_MASKING_FACTOR / (double) ((1 << (i + 1)) - 1) * scales[i];
    }
}

static void unpack_frame(uint8_t *buf, int buf_size, float *freq1, float *freq2, const float* scales, uint8_t *exponents, int freq_size) {
    uint8_t flags, cb_bits, ev_bits, ei_bits, qv_bits;
    int qv;
    uint8_t ev;
    uint8_t move, pos;
    uint32_t bit_offset;
    int i, j;
    int freq_half = freq_size >> 1;


    memset(freq1, 0, RELIC_MAX_FREQ * sizeof(float));
    memset(freq2, 0, RELIC_MAX_FREQ * sizeof(float));

    flags   = read_ubits(2u, 0u, buf);
    cb_bits = read_ubits(3u, 2u, buf);
    ev_bits = read_ubits(2u, 5u, buf);
    ei_bits = read_ubits(4u, 7u, buf);
    bit_offset = 11;

    /* reset exponents indexes */
    if ((flags & 1) == 1) {
        memset(exponents, 0, RELIC_MAX_FREQ);
    }

    /* read packed exponents indexes for all bands */
    if (cb_bits > 0 && ev_bits > 0) {
        pos = 0;
        for (i = 0; i < RELIC_CRITICAL_BAND_COUNT - 1; i++) {
            if (bit_offset >= 8u*buf_size)
                break;

            move = read_ubits(cb_bits, bit_offset, buf);
            bit_offset += cb_bits;
            if (i > 0 && move == 0)
                break;
            pos += move;

            ev = read_ubits(ev_bits, bit_offset, buf);
            bit_offset += ev_bits;
            for (j = critical_band_data[pos]; j < critical_band_data[pos + 1]; j++)
                exponents[j] = ev;
        }
    }
    
    /* read quantized values */
    if (freq_half > 0 && ei_bits > 0) {

        /* read first part */
        pos = 0;
        for (i = 0; i < RELIC_MAX_FREQ; i++) {
            if (bit_offset >= 8u*buf_size)
                break;

            move = read_ubits(ei_bits, bit_offset, buf);
            bit_offset += ei_bits;
            if (i > 0 && move == 0)
                break;
            pos += move;

            qv_bits = exponents[pos];
            qv = read_sbits(qv_bits + 2u, bit_offset, buf);
            bit_offset += qv_bits + 2u;

            if (qv != 0 && pos < freq_half && qv_bits < 6)
                freq1[pos] = (float)qv * scales[qv_bits];
        }

        /* read second part, or clone it */
        if ((flags & 2) == 2) {                     
            memcpy(freq2, freq1, RELIC_MAX_FREQ * sizeof(float));
        }
        else {
            pos = 0;
            for (i = 0; i < RELIC_MAX_FREQ; i++) {
                if (bit_offset >= 8u*buf_size)
                    break;

                move = read_ubits(ei_bits, bit_offset, buf);
                bit_offset += ei_bits;
                if (i > 0 && move == 0)
                    break;
                pos += move;

                qv_bits = exponents[pos];
                qv = read_sbits(qv_bits + 2u, bit_offset, buf);
                bit_offset += qv_bits + 2u;

                if (qv != 0 && pos < freq_half && qv_bits < 6)
                    freq2[pos] = (float)qv * scales[qv_bits];
            }
        }
    }
}

static int decode_frame_next(VGMSTREAMCHANNEL* stream, relic_codec_data* data) {
    int ch;
    int bytes;
    uint8_t buf[RELIC_MAX_FRAME_SIZE];
    
    for (ch = 0; ch < data->channels; ch++) {
        /* clean extra bytes for bitreader */
        memset(buf + data->frame_size, 0, RELIC_MAX_FRAME_SIZE - data->frame_size);

        bytes = read_streamfile(buf, stream->offset, data->frame_size, stream->streamfile);
        if (bytes != data->frame_size) goto fail;
        stream->offset += data->frame_size;
                    
        unpack_frame(buf, sizeof(buf), data->freq1, data->freq2, data->scales, data->exponents[ch], data->freq_size);

        decode_frame_base(data->freq1, data->freq2, data->wave_cur[ch], data->wave_prv[ch], data->dct, data->window, data->dct_mode, data->samples_mode);
    }

    data->samples_consumed = 0;
    data->samples_filled = data->wave_size;
    return 1;
fail:
    return 0;
}

static void copy_samples(relic_codec_data* data, sample_t* outbuf, int32_t samples) {
    int s, ch;
    int ichs = data->channels;
    int skip = data->samples_consumed;
    for (ch = 0; ch < ichs; ch++) {
        for (s = 0; s < samples; s++) {
            double d64_sample = data->wave_cur[ch][skip + s];
            int pcm_sample = clamp16((int32_t)d64_sample);

            /* f32 in PCM 32767.0 .. -32768.0 format, original code
             * does some custom double-to-int rint() though */
            //FQ_BNUM ((float)(1<<26)*(1<<26)*1.5)
            //rint(x) ((d64 = (double)(x)+FQ_BNUM), *(int*)(&d64))

            outbuf[s*ichs + ch] = pcm_sample;
        }
    }
}

static relic_codec_data* init_codec(int channels, int bitrate, int codec_rate) {
    relic_codec_data *data = NULL;

    if (channels > RELIC_MAX_CHANNELS)
        goto fail;

    data = calloc(1, sizeof(relic_codec_data));
    if (!data) goto fail;

    data->channels = channels;

    /* dequantized freq1+2 size (separate from DCT) */
    if (codec_rate < 22050) /* probably 11025 only */
        data->freq_size = RELIC_SIZE_LOW;
    if (codec_rate == 22050)
        data->freq_size = RELIC_SIZE_MID;
    if (codec_rate > 22050) /* probably 44100 only */
        data->freq_size = RELIC_SIZE_HIGH;

    /* default for streams (only a few mode combos are valid, see decode) */
    data->wave_size = RELIC_SIZE_HIGH;
    data->dct_mode = RELIC_SIZE_HIGH;
    data->samples_mode = RELIC_SIZE_HIGH;

    init_dct(data->dct, RELIC_SIZE_HIGH);
    init_window(data->window, RELIC_SIZE_HIGH);
    init_dequantization(data->scales);
    memset(data->wave_prv, 0, RELIC_MAX_CHANNELS * RELIC_MAX_SIZE * sizeof(float));

    switch(bitrate) {
        case RELIC_BITRATE_22:
        case RELIC_BITRATE_44:
        case RELIC_BITRATE_88:
        case RELIC_BITRATE_176:
            data->frame_size = (bitrate / 8); /* 0x100 and 0x80 are common */
            break;
        default:
            goto fail;
    }


    return data;
fail:
    free_relic(data);
    return NULL;
}

static void reset_codec(relic_codec_data* data) {
    memset(data->wave_prv, 0, RELIC_MAX_CHANNELS * RELIC_MAX_SIZE * sizeof(float));
}
