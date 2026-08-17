/**
 * clHCA DECODER
 *
 * Decodes CRI's HCA (High Compression Audio), a CBR DCT-based codec (similar to AAC).
 * Also supports what CRI calls HCA-MX, which basically is the same thing with constrained
 * encoder settings.
 *
 * - Original decompilation and C++ decoder by nyaga
 *     https://github.com/Nyagamon/HCADecoder
 * - Ported to C by kode54
 *     https://gist.github.com/kode54/ce2bf799b445002e125f06ed833903c0
 * - Cleaned up and re-reverse engineered for HCA v3 by bnnm, using Thealexbarney's VGAudio decoder as reference
 *     https://github.com/Thealexbarney/VGAudio
 */

/* TODO:
 * - improve portability on types and float casts, sizeof(int) isn't necessarily sizeof(float)
 * - simplify DCT4 code
 * - add extra validations: encoder_delay/padding < sample_count, etc
 * - intensity should memset if intensity is 15 or set in reset? (no games hit 15?)
 * - check mdct + tables, add floats
 * - simplify bitreader to use in decoder only (no need to read +16 bits)
 */

//--------------------------------------------------
// Includes
//--------------------------------------------------
#include "clhca.h"
#include "clhca_data.h"
#include <stddef.h>
#include <stdlib.h>
#include <memory.h>

/* CRI libs may only accept last version in some cases/modes, though most decoding takes older versions
 * into account. Lib is identified with "HCA Decoder (Float)" + version string. Some known versions:
 * - ~V1.1 2011 [first public version]
 * - ~V1.2 2011 [ciph/ath chunks, disabled ATH]
 * - Ver.1.40 2011-04 [header mask]
 * - Ver.1.42 2011-05
 * - Ver.1.45.02 2012-03
 * - Ver.2.00.02 2012-06 [decoding updates]
 * - Ver.2.02.02 2013-12, 2014-11
 * - Ver.2.06.05 2018-12 [scramble subkey API]
 * - Ver.2.06.07 2020-02, 2021-02
 * - Ver.3.01.00 2020-11 [decoding updates]
 * Same version rebuilt gets a newer date, and new APIs change header strings, but header versions
 * only change when decoder does. Despite the name, no "Integer" version seems to exist.
 */
#define HCA_VERSION_V101 0x0101 /* V1.1+ [El Shaddai (PS3/X360)] */
#define HCA_VERSION_V102 0x0102 /* V1.2+ [Gekka Ryouran Romance (PSP)] */
#define HCA_VERSION_V103 0x0103 /* V1.4+ [Phantasy Star Online 2 (PC), Binary Domain (PS3)] */
#define HCA_VERSION_V200 0x0200 /* V2.0+ [Yakuza 5 (PS3)] */
#define HCA_VERSION_V300 0x0300 /* V3.0+ [Uma Musume (Android), Megaton Musashi (Switch)-sfx-hfrgroups] */

/* maxs depend on encoder quality settings (for example, stereo has:
 * highest=0x400, high=0x2AA, medium=0x200, low=0x155, lowest=0x100) */
#define HCA_MIN_FRAME_SIZE 0x8          /* lib min */
#define HCA_MAX_FRAME_SIZE 0xFFFF       /* lib max */

#define HCA_MASK  0x7F7F7F7F            /* chunk obfuscation when the HCA is encrypted with key */
#define HCA_SUBFRAMES  8
#define HCA_SAMPLES_PER_SUBFRAME  128   /* also spectrum points/etc */
#define HCA_SAMPLES_PER_FRAME  (HCA_SUBFRAMES * HCA_SAMPLES_PER_SUBFRAME)
#define HCA_MDCT_BITS  7                /* (1<<7) = 128 */

#define HCA_MIN_CHANNELS  1
#define HCA_MAX_CHANNELS  16            /* internal max (in practice only 8 can be encoded) */
#define HCA_MIN_SAMPLE_RATE  1          /* assumed */
#define HCA_MAX_SAMPLE_RATE  0x7FFFFF   /* encoder max seems 48000 */

#define HCA_DEFAULT_RANDOM  1

#define HCA_RESULT_OK            0
#define HCA_ERROR_PARAMS        -1
#define HCA_ERROR_HEADER        -2
#define HCA_ERROR_CHECKSUM      -3
#define HCA_ERROR_SYNC          -4
#define HCA_ERROR_UNPACK        -5
#define HCA_ERROR_BITREADER     -6

//--------------------------------------------------
// Decoder config/state
//--------------------------------------------------
typedef enum { DISCRETE = 0, STEREO_PRIMARY = 1, STEREO_SECONDARY = 2 } channel_type_t;


/* lib only saves 1 spectra subframe at a time, and original order is:
 *  spectra, gain, scalefactors, resolution, noises, intensity, dirty_flag, type, coded/valid/noise_count */
typedef struct stChannel {
    /* HCA channel config */
    channel_type_t type;
    unsigned int coded_count;                               /* encoded scales/resolutions/coefs */

    /* subframe state */
    unsigned char intensity[HCA_SUBFRAMES];                 /* intensity indexes for joins stereo (value max: 15 / 4b) */
    unsigned char scalefactors[HCA_SAMPLES_PER_SUBFRAME];   /* scale indexes (value max: 64 / 6b)*/
    unsigned char resolution[HCA_SAMPLES_PER_SUBFRAME];     /* resolution indexes (value max: 15 / 4b) */
    unsigned char noises[HCA_SAMPLES_PER_SUBFRAME];         /* indexes to coefs that need noise fill + coefs that don't (value max: 128 / 8b) */
    unsigned int noise_count;                               /* resolutions with noise values saved in 'noises' */
    unsigned int valid_count;                               /* resolutions with valid values saved in 'noises' */

    float gain[HCA_SAMPLES_PER_SUBFRAME];                   /* gain to apply to quantized spectral data */
    float spectra[HCA_SUBFRAMES][HCA_SAMPLES_PER_SUBFRAME]; /* resulting dequantized data */

    float temp[HCA_SAMPLES_PER_SUBFRAME];                   /* temp for DCT-IV */
    float dct[HCA_SAMPLES_PER_SUBFRAME];                    /* result of DCT-IV */
    float imdct_previous[HCA_SAMPLES_PER_SUBFRAME];         /* IMDCT */

    /* frame state */
    float wave[HCA_SUBFRAMES][HCA_SAMPLES_PER_SUBFRAME];  /* resulting samples */
} stChannel;

typedef struct clHCA {
    /* header config */
    unsigned int is_valid;
    /* hca chunk */
    unsigned int version;
    unsigned int header_size;
    /* fmt chunk */
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int frame_count;
    unsigned int encoder_delay;
    unsigned int encoder_padding;
    /* comp/dec chunk */
    unsigned int frame_size;
    unsigned int min_resolution;
    unsigned int max_resolution;
    unsigned int track_count;
    unsigned int channel_config;
    unsigned int stereo_type;
    unsigned int total_band_count;
    unsigned int base_band_count;
    unsigned int stereo_band_count;
    unsigned int bands_per_hfr_group;
    unsigned int ms_stereo;
    unsigned int reserved;
    /* vbr chunk */
    unsigned int vbr_max_frame_size;
    unsigned int vbr_noise_Level;
    /* ath chunk */
    unsigned int ath_type;
    /* loop chunk */
    unsigned int loop_start_frame;
    unsigned int loop_end_frame;
    unsigned int loop_start_delay;
    unsigned int loop_end_padding;
    unsigned int loop_flag;
    /* ciph chunk */
    unsigned int ciph_type;
    unsigned long long keycode;
    /* rva chunk */
    float rva_volume;
    /* comm chunk */
    unsigned int comment_len; /* max 0xFF */
    char comment[255+1];

    /* initial state */
    unsigned int hfr_group_count;                       /* high frequency band groups not encoded directly */
    unsigned char ath_curve[HCA_SAMPLES_PER_SUBFRAME];
    unsigned char cipher_table[256];
    /* variable state */
    unsigned int random;
    stChannel channel[HCA_MAX_CHANNELS];
} clHCA;

typedef struct clData {
    const unsigned char* data;
    int size;
    int bit;
} clData;


//--------------------------------------------------
// Checksum
//--------------------------------------------------

//HCACommon_CalculateCrc
static unsigned short crc16_checksum(const unsigned char* data, unsigned int size) {
    unsigned int i;
    unsigned short sum = 0;

    /* HCA header/frames should always have checksum 0 (checksum(size-16b) = last 16b) */
    for (i = 0; i < size; i++) {
        sum = (sum << 8) ^ hcacommon_crc_mask_table[(sum >> 8) ^ data[i]];
    }
    return sum;
}

//--------------------------------------------------
// Bitstream reader
//--------------------------------------------------
static void bitreader_init(clData* br, const void* data, int size) {
    br->data = data;
    br->size = size * 8;
    br->bit = 0;
}

/* CRI's bitreader only handles 16b max during decode (header just reads bytes)
 * so maybe could be optimized by ignoring higher cases */
static unsigned int bitreader_peek(clData* br, int bits_read) {
    const unsigned int bit_pos = br->bit;
    const unsigned int bit_rem = bit_pos & 7;
    const unsigned int bit_size = br->size;
    unsigned int v = 0;
    unsigned int bit_offset, bits_left;

    if (bit_pos + bits_read > bit_size)
        return v;
    if (bits_read == 0) /* may happen when resolution is 0 (dequantize_coefficients) */
        return v;

    bit_offset = bits_read + bit_rem;
    bits_left = bit_size - bit_pos;
    if (bits_left >= 32 && bit_offset >= 25) {
        static const unsigned int mask[8] = {
                0xFFFFFFFF,0x7FFFFFFF,0x3FFFFFFF,0x1FFFFFFF,
                0x0FFFFFFF,0x07FFFFFF,0x03FFFFFF,0x01FFFFFF
        };
        const unsigned char* data = &br->data[bit_pos >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v = (v << 8) | data[2];
        v = (v << 8) | data[3];
        v &= mask[bit_rem];
        v >>= 32 - bit_rem - bits_read;
    }
    else if (bits_left >= 24 && bit_offset >= 17) {
        static const unsigned int mask[8] = {
                0xFFFFFF,0x7FFFFF,0x3FFFFF,0x1FFFFF,
                0x0FFFFF,0x07FFFF,0x03FFFF,0x01FFFF
        };
        const unsigned char* data = &br->data[bit_pos >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v = (v << 8) | data[2];
        v &= mask[bit_rem];
        v >>= 24 - bit_rem - bits_read;
    }
    else if (bits_left >= 16 && bit_offset >= 9) {
        static const unsigned int mask[8] = {
                0xFFFF,0x7FFF,0x3FFF,0x1FFF,0x0FFF,0x07FF,0x03FF,0x01FF
        };
        const unsigned char* data = &br->data[bit_pos >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v &= mask[bit_rem];
        v >>= 16 - bit_rem - bits_read;
    }
    else {
        static const unsigned int mask[8] = {
                0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01
        };
        const unsigned char* data = &br->data[bit_pos >> 3];
        v = data[0];
        v &= mask[bit_rem];
        v >>= 8 - bit_rem - bits_read;
    }
    return v;
}

static unsigned int bitreader_read(clData* br, int bitsize) {
    unsigned int v = bitreader_peek(br, bitsize);
    br->bit += bitsize;
    return v;
}

static void bitreader_skip(clData* br, int bitsize) {
    br->bit += bitsize;
}

//--------------------------------------------------
// API/Utilities
//--------------------------------------------------

int clHCA_isOurFile(const void* data, unsigned int size) {
    clData br;
    unsigned int header_size = 0;

    if (!data || size < 0x08)
        return HCA_ERROR_PARAMS;

    bitreader_init(&br, data, 8);
    if ((bitreader_peek(&br, 32) & HCA_MASK) == 0x48434100) {/*'HCA\0'*/
        bitreader_skip(&br, 32 + 16);
        header_size = bitreader_read(&br, 16);
    }

    if (header_size == 0)
        return HCA_ERROR_HEADER;
    return header_size;
}

int clHCA_getInfo(clHCA* hca, clHCA_stInfo* info) {
    if (!hca || !info || !hca->is_valid)
        return HCA_ERROR_PARAMS;

    info->version = hca->version;
    info->headerSize = hca->header_size;
    info->samplingRate = hca->sample_rate;
    info->channelCount = hca->channels;
    info->blockSize = hca->frame_size;
    info->blockCount = hca->frame_count;
    info->encoderDelay = hca->encoder_delay;
    info->encoderPadding = hca->encoder_padding;
    info->loopEnabled = hca->loop_flag;
    info->loopStartBlock = hca->loop_start_frame;
    info->loopEndBlock = hca->loop_end_frame;
    info->loopStartDelay = hca->loop_start_delay;
    info->loopEndPadding = hca->loop_end_padding;
    info->samplesPerBlock = HCA_SAMPLES_PER_FRAME;
    info->comment = hca->comment;
    info->encryptionEnabled = hca->ciph_type == 56; /* keycode encryption */

    // derived
    info->sampleCount = info->blockCount * info->samplesPerBlock - info->encoderDelay - info->encoderPadding;
    info->loopStartSample = info->loopStartBlock * info->samplesPerBlock - info->encoderDelay + info->loopStartDelay;
    info->loopEndSample = info->loopEndBlock * info->samplesPerBlock - info->encoderDelay + (info->samplesPerBlock - info->loopEndPadding);

    return 0;
}

//HCADecoder_DecodeBlockInt32
void clHCA_ReadSamples16(clHCA* hca, short* samples) {
    const float scale_f = 32768.0f;
    int i;

    /* PCM output is generally unused, but lib functions seem to use SIMD for f32 to s32 + round to zero */
    for (i = 0; i < HCA_SUBFRAMES; i++) {
        for (int j = 0; j < HCA_SAMPLES_PER_SUBFRAME; j++) {
            for (int k = 0; k < hca->channels; k++) {
                float f = hca->channel[k].wave[i][j];
                //f = f * hca->rva_volume; /* rare, won't apply for now */
                int s = (signed int)(f * scale_f);
                if (s > 32767)
                    s = 32767;
                else if (s < -32768)
                    s = -32768;
                *samples++ = (signed short)s;
            }
        }
    }
}

void clHCA_ReadSamples(clHCA* hca, float* samples) {
    int i;

    /* interleave output */
    for (i = 0; i < HCA_SUBFRAMES; i++) {
        for (int j = 0; j < HCA_SAMPLES_PER_SUBFRAME; j++) {
            for (int k = 0; k < hca->channels; k++) {
                float f = hca->channel[k].wave[i][j];
                //f = f * hca->rva_volume; /* rare, won't apply for now */
                *samples++ = f;
            }
        }
    }
}


//--------------------------------------------------
// Allocation and creation
//--------------------------------------------------
static void clHCA_constructor(clHCA* hca) {
    if (!hca)
        return;
    memset(hca, 0, sizeof(*hca));
    hca->is_valid = 0;
}

static void clHCA_destructor(clHCA* hca) {
    hca->is_valid = 0;
}

int clHCA_sizeof(void) {
    return sizeof(clHCA);
}

void clHCA_clear(clHCA* hca) {
    clHCA_constructor(hca);
}

void clHCA_done(clHCA* hca) {
    clHCA_destructor(hca);
}

clHCA* clHCA_new(void) {
    clHCA* hca = malloc(clHCA_sizeof());
    if (hca) {
        clHCA_constructor(hca);
    }
    return hca;
}

void clHCA_delete(clHCA* hca) {
    clHCA_destructor(hca);
    free(hca);
}

//--------------------------------------------------
// ATH
//--------------------------------------------------

static void ath_init0(unsigned char* ath_curve) {
    /* disable curve */
    memset(ath_curve, 0, sizeof(ath_curve[0]) * HCA_SAMPLES_PER_SUBFRAME);
}

static void ath_init1(unsigned char* ath_curve, unsigned int sample_rate) {
    int i;
    unsigned int index;
    unsigned int acc = 0;

    /* scale ATH curve depending on frequency */
    for (i = 0; i < HCA_SAMPLES_PER_SUBFRAME; i++) {
        acc += sample_rate;
        index = acc >> 13;

        if (index >= 654) {
            memset(ath_curve+i, 0xFF, sizeof(ath_curve[0]) * (HCA_SAMPLES_PER_SUBFRAME - i));
            break;
        }
        ath_curve[i] = ath_base_curve[index];
    }
}

static int ath_init(unsigned char* ath_curve, int type, unsigned int sample_rate) {
    switch (type) {
        case 0:
            ath_init0(ath_curve);
            break;
        case 1:
            ath_init1(ath_curve, sample_rate);
            break;
        default:
            return HCA_ERROR_HEADER;
    }
    return HCA_RESULT_OK;
}


//--------------------------------------------------
// Encryption
//--------------------------------------------------
static void cipher_decrypt(unsigned char* cipher_table, unsigned char* data, int size) {
    int i;

    for (i = 0; i < size; i++) {
        data[i] = cipher_table[data[i]];
    }
}

static void cipher_init0(unsigned char* cipher_table) {
    int i;

    /* no encryption */
    for (i = 0; i < 256; i++) {
        cipher_table[i] = i;
    }
}

static void cipher_init1(unsigned char* cipher_table) {
    const int mul = 13;
    const int add = 11;
    int i, v = 0;

    /* keyless encryption (rare) */
    for (i = 1; i < 256 - 1; i++) {
        v = (v * mul + add) & 0xFF;
        if (v == 0 || v == 0xFF)
            v = (v * mul + add) & 0xFF;
        cipher_table[i] = v;
    }
    cipher_table[0] = 0;
    cipher_table[0xFF] = 0xFF;
}

static void cipher_init56_create_table(unsigned char* r, unsigned char key) {
    const int mul = ((key & 1) << 3) | 5;
    const int add = (key & 0xE) | 1;
    int i;

    key >>= 4;
    for (i = 0; i < 16; i++) {
        key = (key * mul + add) & 0xF;
        r[i] = key;
    }
}

static void cipher_init56(unsigned char* cipher_table, unsigned long long keycode) {
    unsigned char kc[8];
    unsigned char seed[16];
    unsigned char base[256], base_r[16], base_c[16];
    int r, c;

    /* 56bit keycode encryption (given as a uint64_t number, but upper 8b aren't used) */

    /* keycode = keycode - 1 */
    if (keycode != 0)
        keycode--;

    /* init keycode table */
    for (r = 0; r < (8-1); r++) {
        kc[r] = keycode & 0xFF;
        keycode = keycode >> 8;
    }

    /* init seed table */
    seed[0x00] = kc[1];
    seed[0x01] = kc[1] ^ kc[6];
    seed[0x02] = kc[2] ^ kc[3];
    seed[0x03] = kc[2];
    seed[0x04] = kc[2] ^ kc[1];
    seed[0x05] = kc[3] ^ kc[4];
    seed[0x06] = kc[3];
    seed[0x07] = kc[3] ^ kc[2];
    seed[0x08] = kc[4] ^ kc[5];
    seed[0x09] = kc[4];
    seed[0x0A] = kc[4] ^ kc[3];
    seed[0x0B] = kc[5] ^ kc[6];
    seed[0x0C] = kc[5];
    seed[0x0D] = kc[5] ^ kc[4];
    seed[0x0E] = kc[6] ^ kc[1];
    seed[0x0F] = kc[6];

    /* init base table */
    cipher_init56_create_table(base_r, kc[0]);
    for (r = 0; r < 16; r++) {
        unsigned char nb;
        cipher_init56_create_table(base_c, seed[r]);
        nb = base_r[r] << 4;
        for (c = 0; c < 16; c++) {
            base[r*16 + c] = nb | base_c[c]; /* combine nibbles */
        }
    }

    /* final shuffle table */
    {
        int i;
        unsigned int x = 0;
        unsigned int pos = 1;

        for (i = 0; i < 256; i++) {
            x = (x + 17) & 0xFF;
            if (base[x] != 0 && base[x] != 0xFF)
                cipher_table[pos++] = base[x];
        }
        cipher_table[0] = 0;
        cipher_table[0xFF] = 0xFF;
    }
}

static int cipher_init(unsigned char* cipher_table, int type, unsigned long long keycode) {
    if (type == 56 && keycode == 0)
        type = 0;

    switch (type) {
        case 0:
            cipher_init0(cipher_table);
            break;
        case 1:
            cipher_init1(cipher_table);
            break;
        case 56:
            cipher_init56(cipher_table, keycode);
            break;
        default:
            return HCA_ERROR_HEADER;
    }
    return HCA_RESULT_OK;
}

//--------------------------------------------------
// Parse
//--------------------------------------------------
static unsigned int header_ceil2(unsigned int a, unsigned int b) {
    if (b < 1)
        return 0;
    return (a / b + ((a % b) ? 1 : 0)); /* lib modulo: a - (a / b * b) */
}


/* setup config based on header info */
// HCAHeaderUtility_GetElementTypes
static void setup_channel_types(int channels, int track_count, int channel_config, int stereo_band_count, channel_type_t* channel_types) {
    unsigned int i, channels_per_track;

    channels_per_track = channels / track_count;
    if (stereo_band_count > 0 && channels_per_track > 1) {
        channel_type_t* ct = channel_types;
        for (i = 0; i < track_count; i++, ct += channels_per_track) {
            switch (channels_per_track) {
                case 2:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    break;
                case 3:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    ct[2] = DISCRETE;
                    break;
                case 4:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    if (channel_config == 0) {
                        ct[2] = STEREO_PRIMARY;
                        ct[3] = STEREO_SECONDARY;
                    } else {
                        ct[2] = DISCRETE;
                        ct[3] = DISCRETE;
                    }
                    break;
                case 5:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    ct[2] = DISCRETE;
                    if (channel_config <= 2) {
                        ct[3] = STEREO_PRIMARY;
                        ct[4] = STEREO_SECONDARY;
                    } else {
                        ct[3] = DISCRETE;
                        ct[4] = DISCRETE;
                    }
                    break;
                case 6:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    ct[2] = DISCRETE;
                    ct[3] = DISCRETE;
                    ct[4] = STEREO_PRIMARY;
                    ct[5] = STEREO_SECONDARY;
                    break;
                case 7:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    ct[2] = DISCRETE;
                    ct[3] = DISCRETE;
                    ct[4] = STEREO_PRIMARY;
                    ct[5] = STEREO_SECONDARY;
                    ct[6] = DISCRETE;
                    break;
                case 8:
                    ct[0] = STEREO_PRIMARY;
                    ct[1] = STEREO_SECONDARY;
                    ct[2] = DISCRETE;
                    ct[3] = DISCRETE;
                    ct[4] = STEREO_PRIMARY;
                    ct[5] = STEREO_SECONDARY;
                    ct[6] = STEREO_PRIMARY;
                    ct[7] = STEREO_SECONDARY;
                    break;
                default:
                    /* all 0 (DISCRETE) */
                    //for (ch = 0; ch < channels_per_track; ch++) {
                    //    ct[i] = DISCRETE;
                    //}
                    break;
            }
        }
    }

    /* lib sets to 0 after channels_per_track * track_count 
     * (implicit here since channel_types is init'd to 0) */
}

int clHCA_DecodeHeader(clHCA* hca, const void* data, unsigned int size) {
    clData br;
    int res;

    if (!hca || !data)
        return HCA_ERROR_PARAMS;

    hca->is_valid = 0;

    if (size < 0x08)
        return HCA_ERROR_PARAMS;

    bitreader_init(&br, data, size);

    /* read header chunks (in HCA chunks must follow a fixed order) */

    /* HCA base header */
    if ((bitreader_peek(&br, 32) & HCA_MASK) == 0x48434100) { /* "HCA\0" */
        bitreader_skip(&br, 32);
        hca->version = bitreader_read(&br, 16); /* lib reads as version + subversion (uses main version for feature checks) */
        hca->header_size = bitreader_read(&br, 16);

        if (hca->version != HCA_VERSION_V101 &&
            hca->version != HCA_VERSION_V102 &&
            hca->version != HCA_VERSION_V103 &&
            hca->version != HCA_VERSION_V200 &&
            hca->version != HCA_VERSION_V300)
            return HCA_ERROR_HEADER;

        if (size < hca->header_size)
            return HCA_ERROR_PARAMS;

        if (crc16_checksum(data,hca->header_size))
            return HCA_ERROR_CHECKSUM;

        size -= 0x08;
    }
    else {
        return HCA_ERROR_HEADER;
    }

    /* format info */
    if (size >= 0x10 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x666D7400) { /* "fmt\0" */
        bitreader_skip(&br, 32);
        hca->channels = bitreader_read(&br, 8);
        hca->sample_rate = bitreader_read(&br, 24);
        hca->frame_count = bitreader_read(&br, 32);
        hca->encoder_delay = bitreader_read(&br, 16);
        hca->encoder_padding = bitreader_read(&br, 16);

        if (!(hca->channels >= HCA_MIN_CHANNELS && hca->channels <= HCA_MAX_CHANNELS))
            return HCA_ERROR_HEADER;

        if (hca->frame_count == 0)
            return HCA_ERROR_HEADER;

        if (!(hca->sample_rate >= HCA_MIN_SAMPLE_RATE && hca->sample_rate <= HCA_MAX_SAMPLE_RATE))
            return HCA_ERROR_HEADER;

        size -= 0x10;
    }
    else {
        return HCA_ERROR_HEADER;
    }

    /* compression (v2.0) or decode (v1.x) info */
    if (size >= 0x10 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x636F6D70) { /* "comp" */
        bitreader_skip(&br, 32);
        hca->frame_size = bitreader_read(&br, 16);
        hca->min_resolution = bitreader_read(&br, 8);
        hca->max_resolution = bitreader_read(&br, 8);
        hca->track_count = bitreader_read(&br, 8);
        hca->channel_config = bitreader_read(&br, 8);
        hca->total_band_count = bitreader_read(&br, 8);
        hca->base_band_count = bitreader_read(&br, 8);
        hca->stereo_band_count = bitreader_read(&br, 8);
        hca->bands_per_hfr_group = bitreader_read(&br, 8);
        hca->ms_stereo = bitreader_read(&br, 8);
        hca->reserved = bitreader_read(&br, 8); /* not actually read by lib */

        size -= 0x10;
    }
    else if (size >= 0x0c && (bitreader_peek(&br, 32) & HCA_MASK) == 0x64656300) { /* "dec\0" */
        bitreader_skip(&br, 32);
        hca->frame_size = bitreader_read(&br, 16);
        hca->min_resolution = bitreader_read(&br, 8);
        hca->max_resolution = bitreader_read(&br, 8);
        hca->total_band_count = bitreader_read(&br, 8) + 1;
        hca->base_band_count = bitreader_read(&br, 8) + 1;
        hca->track_count = bitreader_read(&br, 4);
        hca->channel_config = bitreader_read(&br, 4);
        hca->stereo_type = bitreader_read(&br, 8);

        if (hca->stereo_type == 0)
            hca->base_band_count = hca->total_band_count;
        hca->stereo_band_count = hca->total_band_count - hca->base_band_count;
        hca->bands_per_hfr_group = 0;

        size -= 0x0c;
    }
    else {
        return HCA_ERROR_HEADER;
    }

    /* VBR (variable bit rate) info */
    if (size >= 0x08 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x76627200) { /* "vbr\0" */
        bitreader_skip(&br, 32);
        hca->vbr_max_frame_size = bitreader_read(&br, 16);
        hca->vbr_noise_Level = bitreader_read(&br, 16);

        if (!(hca->frame_size == 0 && hca->vbr_max_frame_size > 8 && hca->vbr_max_frame_size <= 0x1FF))
            return HCA_ERROR_HEADER;

        size -= 0x08;
    }
    else {
        /* removed in v2.0, probably unused in v1.x */
        hca->vbr_max_frame_size = 0;
        hca->vbr_noise_Level = 0;
    }

    /* ATH (Absolute Threshold of Hearing) info */
    if (size >= 0x06 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x61746800) { /* "ath\0" */
        bitreader_skip(&br, 32);
        hca->ath_type = bitreader_read(&br, 16);
    }
    else {
        /* removed in v2.0, default in v1.x (only used in v1.1, as v1.2/v1.3 set ath_type = 0) */
        hca->ath_type = (hca->version < HCA_VERSION_V200) ? 1 : 0;
    }

    /* loop info */
    if (size >= 0x10 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x6C6F6F70) { /* "loop" */
        bitreader_skip(&br, 32);
        hca->loop_start_frame = bitreader_read(&br, 32);
        hca->loop_end_frame = bitreader_read(&br, 32);
        hca->loop_start_delay = bitreader_read(&br, 16);
        hca->loop_end_padding = bitreader_read(&br, 16);

        hca->loop_flag = 1;

        if (!(hca->loop_start_frame >= 0 && hca->loop_start_frame <= hca->loop_end_frame
                && hca->loop_end_frame < hca->frame_count))
            return HCA_ERROR_HEADER;

        size -= 0x10;
    }
    else {
        hca->loop_start_frame = 0;
        hca->loop_end_frame = 0;
        hca->loop_start_delay = 0;
        hca->loop_end_padding = 0;

        hca->loop_flag = 0;
    }

    /* cipher/encryption info */
    if (size >= 0x06 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x63697068) { /* "ciph" */
        bitreader_skip(&br, 32);
        hca->ciph_type = bitreader_read(&br, 16);

        if (!(hca->ciph_type == 0 || hca->ciph_type == 1 || hca->ciph_type == 56))
            return HCA_ERROR_HEADER;

        size -= 0x06;
    }
    else {
        hca->ciph_type = 0;
    }

    /* RVA (relative volume adjustment) info */
    if (size >= 0x08 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x72766100) { /* "rva\0" */
        union {
            unsigned int i;
            float f;
        } rva_volume_cast;
        bitreader_skip(&br, 32);
        rva_volume_cast.i = bitreader_read(&br, 32);
        hca->rva_volume = rva_volume_cast.f;

        size -= 0x08;
    } else {
        hca->rva_volume = 1.0f; /* encoder volume setting is pre-applied to data, though chunk still exists in +v3.0 */
    }

    /* comment */
    if (size >= 0x05 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x636F6D6D) {/* "comm" */
        unsigned int i;
        bitreader_skip(&br, 32);
        hca->comment_len = bitreader_read(&br, 8);

        if (hca->comment_len > size)
            return HCA_ERROR_HEADER;

        for (i = 0; i < hca->comment_len; ++i)
            hca->comment[i] = bitreader_read(&br, 8);
        hca->comment[i] = '\0'; /* should be null terminated but make sure */

        size -= 0x05 + hca->comment_len;
    }
    else {
        hca->comment_len = 0;
    }

    /* padding info */
    if (size >= 0x04 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x70616400) { /* "pad\0" */
        size -= (size - 0x02); /* fills up to header_size, sans checksum */
    }

    /* should be fully read, but allow as data buffer may be bigger than header_size */
    //if (size != 0x02)
    //    return HCA_ERROR_HEADER;


    /* extra validations */
    if (!(hca->frame_size >= HCA_MIN_FRAME_SIZE && hca->frame_size <= HCA_MAX_FRAME_SIZE)) /* actual max seems 0x155*channels */
        return HCA_ERROR_HEADER; /* theoretically can be 0 if VBR (not seen) */

    if (hca->version <= HCA_VERSION_V200) {
        if (hca->min_resolution != 1 || hca->max_resolution != 15)
            return HCA_ERROR_HEADER;
    }
    else {
        if (hca->min_resolution > hca->max_resolution || hca->max_resolution > 15) /* header seems to allow 31, but later max is 15 */
            return HCA_ERROR_HEADER;
    }


    /* init state */

    if (hca->track_count == 0)
        hca->track_count = 1; /* as done by lib, can be 0 in old HCAs */

    if (hca->track_count > hca->channels)
        return HCA_ERROR_HEADER;

    /* encoded coefs (up to 128) depend in the encoder's "cutoff" hz option */
    if (hca->total_band_count > HCA_SAMPLES_PER_SUBFRAME || hca->total_band_count == 0 ||
        hca->base_band_count + hca->stereo_band_count > hca->total_band_count ||
        hca->base_band_count + hca->stereo_band_count == 0 ||
        hca->stereo_band_count > hca->base_band_count ||
        hca->bands_per_hfr_group > HCA_SAMPLES_PER_SUBFRAME)
        return HCA_ERROR_HEADER;
    /* not sure if the above validations are actually done in lib, so it could clobber arrays instead
     * (see stChannel comment for order). counts read u8 values so should't overflow */

    /* leftover upper HRF coefs to groups
     * (implicitly base_band_count + stereo_band_count + hfr_group_count <= HCA_SAMPLES_PER_SUBFRAME) */
    hca->hfr_group_count = header_ceil2(
            hca->total_band_count - hca->base_band_count - hca->stereo_band_count,
            hca->bands_per_hfr_group);


    /* init channels */
    {
        int unsigned i;
        channel_type_t channel_types[HCA_MAX_CHANNELS] = {0}; /* part of header struct in lib (uchar) */

        setup_channel_types(hca->channels, hca->track_count, hca->channel_config, hca->stereo_band_count, channel_types);

        memset(hca->channel, 0, sizeof(hca->channel));

        for (i = 0; i < hca->channels; i++) {
            int ch_stereo_band_count;

            if (channel_types[i] == STEREO_SECONDARY)
                ch_stereo_band_count = 0;
            else
                ch_stereo_band_count = hca->stereo_band_count;

            hca->channel[i].coded_count = hca->base_band_count + ch_stereo_band_count;
            hca->channel[i].type = channel_types[i];
        }
    }

    hca->random = HCA_DEFAULT_RANDOM;

    res = ath_init(hca->ath_curve, hca->ath_type, hca->sample_rate);
    if (res < 0)
        return res;

    res = cipher_init(hca->cipher_table, hca->ciph_type, hca->keycode);
    if (res < 0)
        return res;


    //TODO: should work but untested
    if (hca->ms_stereo)
        return HCA_ERROR_HEADER;

    /* clHCA is correctly initialized and decoder state reset
     * (keycode is not changed between calls) */
    hca->is_valid = 1;

    return HCA_RESULT_OK;
}

void clHCA_SetKey(clHCA* hca, unsigned long long keycode) {
    if (!hca)
        return;
    hca->keycode = keycode;

    /* May be called even if clHCA is not valid (header not parsed), as the
     * key will be used during DecodeHeader ciph init. If header was already
     * parsed reinitializes the decryption table using the new key. */
    if (hca->is_valid) {
        /* ignore error since it can't really fail */
        cipher_init(hca->cipher_table, hca->ciph_type, hca->keycode);
    }
}

static int clHCA_DecodeBlock_unpack(clHCA* hca, void* data, unsigned int size);
static void clHCA_DecodeBlock_transform(clHCA* hca);


int clHCA_TestBlock(clHCA* hca, void* data, unsigned int size) {
    const int frame_samples = HCA_SUBFRAMES * HCA_SAMPLES_PER_SUBFRAME;
    const float scale = 32768.0f;
    unsigned int i, ch, sf, s;
    int status;
    int clips = 0, blanks = 0, channel_blanks[HCA_MAX_CHANNELS] = {0};
    const unsigned char* buf = data;
    int overread_bytes = 0;
    int top_score;

    /* first blocks can be empty/silent, check all bytes but sync/crc */
    {
        int is_empty = 1;

        for (i = 0x02; i < size - 0x02; i++) {
            if (buf[i] != 0) {
                is_empty = 0;
                break;
            }
        }

        if (is_empty) {
            return 0;
        }
    }

    /* return if decode fails (happens often with wrong keys due to bad bitstream values) */
    status = clHCA_DecodeBlock_unpack(hca, data, size);
    if (status < 0)
        return -1;


    /* detect data errors */
    {
        int bits_max = size * 8;
        int bits_used = status;
        int byte_start;

        /* Should read all frame sans end checksum (16b), but allow 14b as one frame was found to
         * read up to that (cross referenced with CRI's tools), perhaps some encoding hiccup
         * [World of Final Fantasy Maxima (Switch) am_ev21_0170 video] */
        if (bits_used + 14 > bits_max)
            return HCA_ERROR_BITREADER;

        /* Data after reading bits is null (up to end 16b checksum) before/after decryption, so bad
         * keys give garbage beyond those bits (data is decrypted at this point and size >= frame_size) */
        byte_start = (bits_used / 8) + (bits_used % 8 ? 0x01 : 0);
        /* maybe should memcmp with a null frame, unsure of max though, and in most cases
         * should fail fast (this check catches almost everything) */
        for (i = byte_start; i < size - 0x02; i++) {
            if (buf[i] != 0x00) {
                return -1;
            }
        }

        /* Decoder leaves unused frame data as blank. If the current position is in the middle of blank data
         * it may have read a bit too much due to wrong decrypted values. Good keys also sometimes 'overread'
         * a bit in silent-ish frames though, so don't reject outright. */
        for (i = byte_start; i > 0x02; i--) {
            if (buf[i] != 0x00)
                break;
            overread_bytes++;
        }
    }

    /* Other possibly useful checks (also see https://github.com/Youjose/CriCodecs)
     * - frame_acceptable_noise_level <= 255: doesn't seem helpful with keys that look ok-ish but are wrong
     * - number of delta scalefactors >= 64: directly failing may be better? (capped in v3.0 but surely not allowed)
     */


    /* check decode results as (rarely) bad keys may still get here */
    clHCA_DecodeBlock_transform(hca);
    for (ch = 0; ch < hca->channels; ch++) {
        for (sf = 0; sf < HCA_SUBFRAMES; sf++) {
            for (s = 0; s < HCA_SAMPLES_PER_SUBFRAME; s++) {
                float fsample = hca->channel[ch].wave[sf][s];

                if (fsample > 1.0f || fsample < -1.0f) { //improve?
                    clips++;
                }
                else {
                    signed int psample = (signed int) (fsample * scale);
                    if (psample == 0 || psample == -1) {
                        blanks++;
                        channel_blanks[ch]++;
                    }
                }
            }
        }
    }

    /* if block is silent result is not useful */
    if (blanks == hca->channels * frame_samples)
        return 0;

    /* at this point key looks good enough (closer to 1 is better) */
    top_score = 1;

    /* the more clips the less likely block was correctly decrypted */
    if (clips >= 1)
        top_score += clips;

    /* Some bad keys make left channel null and right normal enough (due to joint stereo stuff).
     * It's possible real keys could do this, but don't give full marks just in case. */
    if (hca->channels >= 2) {
        /* only check main L/R, other channels like BL/BR are probably not useful */
        if (channel_blanks[0] == frame_samples && channel_blanks[1] != frame_samples) /* maybe should check max/min values? */
            top_score += 3;
    }

    //TODO: better calcs (would need to know how much non-blank data is in the frame)
    /* More than a few blank frames is a bit odd, make score worse to let other keys kick in.
     * Bad key may overreaad only 2 bytes and give score 1 otherwise, but good keys may also overread 1.
     */
    if (overread_bytes > 1) {
        int score = overread_bytes;
        if (score > 15)
            score = 15;
        top_score += score;
    }

    /* block may be correct (but wrong keys can get this too and should test more blocks) */
    return top_score;
}

void clHCA_DecodeReset(clHCA* hca) {
    unsigned int i;

    if (!hca || !hca->is_valid)
        return;

    hca->random = HCA_DEFAULT_RANDOM;

    for (i = 0; i < hca->channels; i++) {
        stChannel* ch = &hca->channel[i];

        /* most values get overwritten during decode */
        //memset(ch->intensity, 0, sizeof(ch->intensity[0]) * HCA_SUBFRAMES);
        //memset(ch->scalefactors, 0, sizeof(ch->scalefactors[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->resolution, 0, sizeof(ch->resolution[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->gain, 0, sizeof(ch->gain[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->spectra, 0, sizeof(ch->spectra[0]) * HCA_SUBFRAMES * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->temp, 0, sizeof(ch->temp[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->dct, 0, sizeof(ch->dct[0]) * HCA_SAMPLES_PER_SUBFRAME);
        memset(ch->imdct_previous, 0, sizeof(ch->imdct_previous[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->wave, 0, sizeof(ch->wave[0][0]) * HCA_SUBFRAMES * HCA_SUBFRAMES);
    }
}

//--------------------------------------------------
// Decode
//--------------------------------------------------
static int unpack_scalefactors(stChannel* ch, clData* br, unsigned int hfr_group_count, unsigned int version);

static int unpack_intensity(stChannel* ch, clData* br, unsigned int hfr_group_count, unsigned int version);

static void calculate_resolution(stChannel* ch, unsigned int packed_noise_level, const unsigned char* ath_curve,
    unsigned int min_resolution, unsigned int max_resolution);

static void calculate_gain(stChannel* ch);

static void dequantize_coefficients(stChannel* ch, clData* br, int subframe);

static void reconstruct_noise(stChannel* ch, unsigned int min_resolution, unsigned int ms_stereo, unsigned int* random_p, int subframe);

static void reconstruct_high_frequency(stChannel* ch, unsigned int hfr_group_count, unsigned int bands_per_hfr_group,
        unsigned int stereo_band_count, unsigned int base_band_count, unsigned int total_band_count, unsigned int version, int subframe);

static void apply_intensity_stereo(stChannel* ch_pair, int subframe, unsigned int base_band_count, unsigned int total_band_count);

static void apply_ms_stereo(stChannel* ch_pair, unsigned int ms_stereo, unsigned int base_band_count, unsigned int total_band_count, int subframe);

static void imdct_transform(stChannel* ch, int subframe);


static int clHCA_DecodeBlock_unpack(clHCA* hca, void* data, unsigned int size) {
    clData br;
    unsigned short sync;
    unsigned int subframe, ch;

    if (!data || !hca || !hca->is_valid)
        return HCA_ERROR_PARAMS;
    if (size < hca->frame_size)
        return HCA_ERROR_PARAMS;

    bitreader_init(&br, data, hca->frame_size);

    /* test sync (not encrypted) */
    sync = bitreader_read(&br, 16);
    if (sync != 0xFFFF)
        return HCA_ERROR_SYNC;

    if (crc16_checksum(data, hca->frame_size))
        return HCA_ERROR_CHECKSUM;

    cipher_decrypt(hca->cipher_table, data, hca->frame_size);


    /* unpack frame values */
    {
        /* lib saves this in the struct since they can stop/resume subframe decoding */
        unsigned int frame_acceptable_noise_level = bitreader_read(&br, 9);
        unsigned int frame_evaluation_boundary = bitreader_read(&br, 7);

        unsigned int packed_noise_level = (frame_acceptable_noise_level << 8) - frame_evaluation_boundary;

        for (ch = 0; ch < hca->channels; ch++) {
            int err = unpack_scalefactors(&hca->channel[ch], &br, hca->hfr_group_count, hca->version);
            if (err < 0)
                return err;

            unpack_intensity(&hca->channel[ch], &br, hca->hfr_group_count, hca->version);

            calculate_resolution(&hca->channel[ch], packed_noise_level, hca->ath_curve, hca->min_resolution, hca->max_resolution);

            calculate_gain(&hca->channel[ch]);
        }
    }

    /* lib seems to use a state value to skip parts (unpacking/subframe N/etc) as needed */
    for (subframe = 0; subframe < HCA_SUBFRAMES; subframe++) {

        /* unpack channel data and get dequantized spectra */
        for (ch = 0; ch < hca->channels; ch++){
            dequantize_coefficients(&hca->channel[ch], &br, subframe);
        }

        /* original code transforms subframe here, but we have it for later */
    }

    return br.bit; /* numbers of read bits for validations */
}

static void clHCA_DecodeBlock_transform(clHCA* hca) {
    unsigned int subframe, ch;

    for (subframe = 0; subframe < HCA_SUBFRAMES; subframe++) {
        /* restore missing bands from spectra */
        for (ch = 0; ch < hca->channels; ch++) {
            reconstruct_noise(&hca->channel[ch], hca->min_resolution, hca->ms_stereo, &hca->random, subframe);

            reconstruct_high_frequency(&hca->channel[ch], hca->hfr_group_count, hca->bands_per_hfr_group,
                    hca->stereo_band_count, hca->base_band_count, hca->total_band_count, hca->version, subframe);
        }

        /* restore missing joint stereo bands */
        if (hca->stereo_band_count > 0) {
            for (ch = 0; ch < hca->channels - 1; ch++) {
                apply_intensity_stereo(&hca->channel[ch], subframe, hca->base_band_count, hca->total_band_count);

                apply_ms_stereo(&hca->channel[ch], hca->ms_stereo, hca->base_band_count, hca->total_band_count, subframe);
            }
        }

        /* apply imdct */
        for (ch = 0; ch < hca->channels; ch++) {
            imdct_transform(&hca->channel[ch], subframe);
        }
    }
}


/* takes HCA data and decodes all of a frame's samples */
//hcadecoder_decode_block
int clHCA_DecodeBlock(clHCA* hca, void* data, unsigned int size) {
    int res;

    /* Original HCA code doesn't separate unpack + transform and instead unpacks data,
     * reads a subframe's spectra, transforms that subframe and continues unpacking.
     *
     * Unpacking first takes a bit more memory (1 spectra per subframe) but test keys faster
     * (since unpack may fail with bad keys we can skip transform). For regular decoding, this
     * way is somehow slightly faster? (~3-5%, extra compiler optimizations with reduced scope?) */

    res = clHCA_DecodeBlock_unpack(hca, data, size);
    if (res < 0)
        return res;
    clHCA_DecodeBlock_transform(hca);

    return res;
}

//--------------------------------------------------
// Decode 1st step
//--------------------------------------------------

/* get scale indexes to normalize dequantized coefficients */
static int unpack_scalefactors(stChannel* ch, clData* br, unsigned int hfr_group_count, unsigned int version) {
    int i;
    unsigned int cs_count = ch->coded_count;
    unsigned int hs_count; /* v3.0 HFR group scalefactors */
    unsigned int sf_count; /* total */

    unsigned char delta_bits = bitreader_read(br, 3);

    /* added in v3.0 */
    if (ch->type == STEREO_SECONDARY || hfr_group_count == 0 || version <= HCA_VERSION_V200) {
        hs_count = 0;
    }
    else {
        hs_count = hfr_group_count;
    }
    sf_count = cs_count + hs_count; /* guaranteed to be > 0 from header validations */

    if (delta_bits >= 6) {
        /* fixed scalefactors */
        unsigned char value = bitreader_read(br, 6); /* same as reading together but closer vs lib */

        ch->scalefactors[0] = value;
        for (i = 1; i < sf_count; i++) {
            ch->scalefactors[i] = bitreader_read(br, 6);
        }
    }
    else if (delta_bits > 0) {
        /* delta scalefactors */
        const unsigned char expected_delta = (1 << delta_bits) - 1;
        unsigned char value = bitreader_read(br, 6);

        ch->scalefactors[0] = value;
        for (i = 1; i < sf_count; i++) {
            unsigned char delta = bitreader_read(br, delta_bits);

            if (delta == expected_delta) {
                value = bitreader_read(br, 6); /* encoded */
            }
            else {
                /* may happen with bad keycodes, scalefactors must be 6b indexes */
                int scalefactor_test = (int)value + ((int)delta - (int)(expected_delta >> 1));
                if (scalefactor_test < 0 || scalefactor_test >= 64) {
                    return HCA_ERROR_UNPACK;
                }

                value = value - (expected_delta >> 1) + delta; /* differential */

                /* v3.0 lib clamps index (check would be more useful for key detection tho) */
                value = value & 0x3F;
                //if (value >= 64)
                //    return HCA_ERROR_UNPACK;
            }
            ch->scalefactors[i] = value;
        }
    }
    else {
        /* no scalefactors */
        for (i = 0; i < HCA_SAMPLES_PER_SUBFRAME; i++) {
            ch->scalefactors[i] = 0;
        }
    }

    /* copy v3.0 HFR scalefactors towards end (seems unnecessary/reserved but perhaps 'extra' is used by reconstruct_noise)
     * ex. is cs=[0..99], hs=[100..110], extra=[111..127] -> cs=[0..99], (hs=[100..110]), extra=[111..116], hs=[117..127] */
    for (i = hs_count; i > 0; i--) {
        ch->scalefactors[HCA_SAMPLES_PER_SUBFRAME - 1 - hs_count + i] = ch->scalefactors[cs_count - 1 + i];
    }

    return HCA_RESULT_OK;
}

/* read intensity (for joint stereo R) or v2.0 high frequency scales (for regular channels) */
static int unpack_intensity(stChannel* ch, clData* br, unsigned int hfr_group_count, unsigned int version) {
    int i;

    if (ch->type == STEREO_SECONDARY) {
        /* read subframe intensity for channel pair (peek first for valid values, not sure why not consumed) */
        if (version <= HCA_VERSION_V200) {
            unsigned char value = bitreader_peek(br, 4);

            ch->intensity[0] = value;
            if (value < 15) {
                bitreader_skip(br, 4);
                for (i = 1; i < HCA_SUBFRAMES; i++) {
                    ch->intensity[i] = bitreader_read(br, 4);
                }
            }
            /* 15 may be an invalid value? index 15 is 0, but may imply "reuse last subframe's intensity".
             * no games seem to use 15 though */
            //else {
            //    return HCA_ERROR_UNPACK;
            //}
        }
        else {
            unsigned char value = bitreader_peek(br, 4);
            unsigned char delta_bits;

            if (value < 15) {
                bitreader_skip(br, 4);

                delta_bits = bitreader_read(br, 2); /* +1 */

                ch->intensity[0] = value;
                if (delta_bits == 3) { /* 3+1 = 4b */
                    /* fixed intensities */
                    for (i = 1; i < HCA_SUBFRAMES; i++) {
                        ch->intensity[i] = bitreader_read(br, 4);
                    }
                }
                else {
                    /* delta intensities */
                    unsigned char bmax = (2 << delta_bits) - 1;
                    unsigned char bits = delta_bits + 1;

                    for (i = 1; i < HCA_SUBFRAMES; i++) {
                        unsigned char delta = bitreader_read(br, bits);
                        if (delta == bmax) {
                            value = bitreader_read(br, 4); /* encoded */
                        }
                        else {
                            value = value - (bmax >> 1) + delta; /* differential */
                            if (value > 15) //TODO: check
                                return HCA_ERROR_UNPACK; /* not done in lib */
                        }

                        ch->intensity[i] = value;
                    }
                }
            }
            else {
                bitreader_skip(br, 4);
                for (i = 0; i < HCA_SUBFRAMES; i++) {
                    ch->intensity[i] = 7;
                }
            }
        }
    }
    else {
        /* read high frequency scalefactors. v3.0 uses derived values in unpack_scalefactors instead. */
        if (version <= HCA_VERSION_V200) {
            /* pointer in v2.0 lib for v2.0 files is after base+stereo bands, while v3.0 lib for v2.0 files
             * is before end (should be equivalent though, as other functions use the new location) */
            //unsigned char* hfr_scales = &ch->scalefactors[base_band_count + stereo_band_count]; /* v2.0 lib */
            unsigned char* hfr_scales = &ch->scalefactors[HCA_SAMPLES_PER_SUBFRAME - hfr_group_count]; /* v3.0 lib */

            for (i = 0; i < hfr_group_count; i++) {
                hfr_scales[i] = bitreader_read(br, 6);
            }
        }
    }

    return HCA_RESULT_OK;
}

/* get resolutions, that determines range of values per encoded spectrum coefficients */
static void calculate_resolution(stChannel* ch, unsigned int packed_noise_level, const unsigned char* ath_curve, unsigned int min_resolution, unsigned int max_resolution) {
    int i;
    unsigned int cr_count = ch->coded_count;
    unsigned int noise_count = 0;
    unsigned int valid_count = 0;

    for (i = 0; i < cr_count; i++) {
        unsigned char new_resolution = 0;
        unsigned char scalefactor = ch->scalefactors[i];

        if (scalefactor > 0) {
            /* ath_curve doesn't exist (not added) in 2.0 lib, so we set curve values 0 in v1.2>= to allow it */
            int noise_level = ath_curve[i] + ((packed_noise_level + i) >> 8);
            int curve_position = noise_level + 1 - ((5 * scalefactor) >> 1);

            /* v2.0<= allows max 56 + sets rest to 1, while v3.0 table has 1 for 57..65 and
             * clamps to min_resolution below, so v2.0 files are still supported */
            if (curve_position < 0) {
                new_resolution = 15;
            }
            else if (curve_position <= 65) {
                new_resolution = hcadecoder_invert_table[curve_position];
            }
            else {
                new_resolution = 0;
            }

            /* added in v3.0 (before, min_resolution was always 1) */
            if (new_resolution > max_resolution)
                new_resolution = max_resolution;
            else if (new_resolution < min_resolution)
                new_resolution = min_resolution;

            /* save resolution 0 (not encoded) indexes (from 0..N), and regular indexes (from N..0) */
            if (new_resolution < 1) {
                ch->noises[noise_count] = i;
                noise_count++;
            }
            else {
                ch->noises[HCA_SAMPLES_PER_SUBFRAME - 1 - valid_count] = i;
                valid_count++;
            }
        }
        ch->resolution[i] = new_resolution;
    }

    ch->noise_count = noise_count;
    ch->valid_count = valid_count;

    memset(&ch->resolution[cr_count], 0, sizeof(ch->resolution[0]) * (HCA_SAMPLES_PER_SUBFRAME - cr_count));
}

/* get actual scales to dequantize based on saved scalefactors */
// HCADequantizer_CalculateGain
static void calculate_gain(stChannel* ch) {
    int i;
    unsigned int cg_count = ch->coded_count;

    for (i = 0; i < cg_count; i++) {
        float scalefactor_scale = hcadequantizer_scaling_table_float[ ch->scalefactors[i] ];
        float resolution_scale = hcadequantizer_range_table_float[ ch->resolution[i] ];
        ch->gain[i] = scalefactor_scale * resolution_scale;
    }
}

//--------------------------------------------------
// Decode 2nd step
//--------------------------------------------------

/* read spectral coefficients in the bitstream */
static void dequantize_coefficients(stChannel* ch, clData* br, int subframe) {
    int i;
    unsigned int cc_count = ch->coded_count;

    for (i = 0; i < cc_count; i++) {
        float qc;
        unsigned char resolution = ch->resolution[i];
        unsigned char bits = hcatbdecoder_max_bit_table[resolution];
        unsigned int code = bitreader_read(br, bits);

        if (resolution > 7) {
            /* parse values in sign-magnitude form (lowest bit = sign) */
            int signed_code = (1 - ((code & 1) << 1)) * (code >> 1); /* move sign from low to up */
            if (signed_code == 0)
                bitreader_skip(br, -1); /* zero uses one less bit since it has no sign */
            qc = (float)signed_code;
        }
        else {
            /* use prefix codebooks for lower resolutions */
            int index = (resolution << 4) + code;
            int skip = hcatbdecoder_read_bit_table[index] - bits;
            bitreader_skip(br, skip);
            qc = hcatbdecoder_read_val_table[index];
        }

        /* dequantize coef with gain */
        ch->spectra[subframe][i] = ch->gain[i] * qc;
    }

    /* clean rest of spectra */
    memset(&ch->spectra[subframe][cc_count], 0, sizeof(ch->spectra[subframe][0]) * (HCA_SAMPLES_PER_SUBFRAME - cc_count));
}


//--------------------------------------------------
// Decode 3rd step
//--------------------------------------------------

/* recreate resolution 0 coefs (not encoded) with pseudo-random noise based on
 * other coefs/scales (probably similar to AAC's perceptual noise substitution) */
static void reconstruct_noise(stChannel* ch, unsigned int min_resolution, unsigned int ms_stereo, unsigned int* random_p, int subframe) {
    int i;

    if (min_resolution > 0) /* added in v3.0 */
        return;
    if (ch->valid_count <= 0 || ch->noise_count <= 0)
        return;
    if (!(!ms_stereo || ch->type == STEREO_PRIMARY))
        return;

    {
        unsigned int random_out;
        int random_index, target_index, source_index, sf_target, sf_source, sc_index;
        unsigned int random = *random_p;

        for (i = 0; i < ch->noise_count; i++) {
            random = 0x343FD * random + 0x269EC3; /* lib uses Borland/MSVC's LCG rand() */
            random_out = (random >> 16) & 0x7FFF;

            random_index = HCA_SAMPLES_PER_SUBFRAME - ch->valid_count + ((random_out * ch->valid_count) >> 15); /* 0..127 noise index */

            /* points to next resolution 0 index and random non-resolution 0 index (within 0..valid) */
            target_index = ch->noises[i];
            source_index = ch->noises[random_index];

            /* get final scale index */
            sf_target = ch->scalefactors[target_index];
            sf_source = ch->scalefactors[source_index];

            sc_index = (sf_target - sf_source + 62) & ~((sf_target - sf_source + 62) >> 31); /* clamp 0..62 */

            ch->spectra[subframe][target_index] = 
                hcadecoder_scale_conversion_table[sc_index] * ch->spectra[subframe][source_index];
        }

        *random_p = random; /* lib saves this in the bitreader, maybe for simplified passing around */
    }
}

/* recreate missing coefs in high bands based on lower bands (probably similar to AAC's spectral band replication) */
static void reconstruct_high_frequency(stChannel* ch, unsigned int hfr_group_count, unsigned int bands_per_hfr_group,
        unsigned int stereo_band_count, unsigned int base_band_count, unsigned int total_band_count, unsigned int version, int subframe) {
    int i, group;

    if (bands_per_hfr_group == 0) /* added in v2.0, skipped in v2.0 files with 0 bands too */
        return;
    if (ch->type == STEREO_SECONDARY)
        return;

    {
        int group_limit;
        int start_band = stereo_band_count + base_band_count;
        int highband = start_band; // guaranteed to be >=
        int lowband = start_band - 1;
        //unsigned char* hfr_scalefactors = &ch->scalefactors[base_band_count + stereo_band_count]; /* v2.0 lib */
        unsigned char* hfr_scalefactors = &ch->scalefactors[128 - hfr_group_count]; /* v3.0 lib */

        if (version <= HCA_VERSION_V200) {
            group_limit = hfr_group_count;
        }
        else {
            group_limit = (hfr_group_count >= 0) ? hfr_group_count : hfr_group_count + 1; /* default 1? (will become 0 below) */
            group_limit = group_limit >> 1; // half
        }

        for (group = 0; group < hfr_group_count; group++) {
            int sc_index;
            /* v3.0 moves lowband towards 0 only until group reachs limit (v2.0 never reaches the limit) */
            int lowband_adjust = (group < group_limit) ? -1 : 1;

            if (highband >= total_band_count || lowband < 0) /* implicit but for completeness vs lib */
                break;

            for (i = 0; i < bands_per_hfr_group; i++) {
                if (highband >= total_band_count || lowband < 0)
                    break;

                sc_index = hfr_scalefactors[group] - ch->scalefactors[lowband] + 63;
                sc_index = sc_index & ~(sc_index >> 31); /* clamped in v3.0 lib (in theory 6b sf are 0..128) */

                ch->spectra[subframe][highband] = hcadecoder_scale_conversion_table[sc_index] * ch->spectra[subframe][lowband];

                highband += 1;
                lowband += lowband_adjust;
            }
        }

        /* last spectrum coefficient is 0 (normally highband = 128, but perhaps could 'break' before) */
        ch->spectra[subframe][highband - 1] = 0.0f;
    }
}

//--------------------------------------------------
// Decode 4th step
//--------------------------------------------------

/* restore L/R bands based on channel coef + panning */
static void apply_intensity_stereo(stChannel* ch_pair, int subframe, unsigned int base_band_count, unsigned int total_band_count) {
    int band;

    if (ch_pair[0].type != STEREO_PRIMARY)
        return;

    {
        int min_band = base_band_count;
        int max_bands = total_band_count; /* upper stereo bands only */

        float ratio_l = hcadecoder_intensity_ratio_table[ ch_pair[1].intensity[subframe] ];
        float ratio_r = 2.0f - ratio_l; /* correct, though other decoders substract 2.0 (it does use 'fsubr 2.0' and such) */
        float* sp_l = &ch_pair[0].spectra[subframe][0];
        float* sp_r = &ch_pair[1].spectra[subframe][0];

        for (band = min_band; band < max_bands; band++) {
            float coef_l = sp_l[band] * ratio_l;
            float coef_r = sp_l[band] * ratio_r;
            sp_l[band] = coef_l;
            sp_r[band] = coef_r;
        }
    }
}

/* restore L/R bands based on mid (L) +/- side (R) differences */
static void apply_ms_stereo(stChannel* ch_pair, unsigned int ms_stereo, unsigned int base_band_count, unsigned int total_band_count, int subframe) {
    int band;

    if (!ms_stereo) /* added in v3.0 */
        return;
    if (ch_pair[0].type != STEREO_PRIMARY)
        return;

    {
        int min_band = 0;
        int max_bands = base_band_count; /* lower base bands only */

        const float ratio = 0.707106769084930419921875f; /* 0x3F3504F3 = 1/sqrt(2) */
        float* sp_l = &ch_pair[0].spectra[subframe][0];
        float* sp_r = &ch_pair[1].spectra[subframe][0];

        for (band = min_band; band < max_bands; band++) {
            float coef_l = (sp_l[band] + sp_r[band]) * ratio;
            float coef_r = (sp_l[band] - sp_r[band]) * ratio;
            sp_l[band] = coef_l;
            sp_r[band] = coef_r;
        }
    }
}

//--------------------------------------------------
// Decode 5th step
//--------------------------------------------------

/* apply DCT-IV to dequantized spectra to get final samples */
//HCAIMDCT_Transform
static void imdct_transform(stChannel* ch, int subframe) {
    static const unsigned int size = HCA_SAMPLES_PER_SUBFRAME;
    static const unsigned int half = HCA_SAMPLES_PER_SUBFRAME / 2;
    static const unsigned int mdct_bits = HCA_MDCT_BITS;
    unsigned int i, j, k;

    /* This IMDCT (supposedly standard) is all too crafty for me to simplify, see VGAudio (Mdct.Dct4).
     * 'HCAIMDCT_Transform' in recent libs seem to use a 'fast MDCT' algorithm for SIMD like this:
     * pre-rotation > FFT > twiddles > post-rotation. Not sure if lib evolved but should be equivalent.
     */

    /* pre-rotation butterflies */
    {
        unsigned int count1 = 1;
        unsigned int count2 = half;
        float* temp1 = &ch->spectra[subframe][0];
        float* temp2 = &ch->temp[0];

        for (i = 0; i < mdct_bits; i++) {
            float* swap;
            float* d1 = &temp2[0];
            float* d2 = &temp2[count2];

            for (j = 0; j < count1; j++) {
                for (k = 0; k < count2; k++) {
                    float a = *(temp1++);
                    float b = *(temp1++);
                    *(d1++) = a + b;
                    *(d2++) = a - b;
                }
                d1 += count2;
                d2 += count2;
            }
            swap = temp1 - HCA_SAMPLES_PER_SUBFRAME; /* move spectra or temp to beginning */
            temp1 = temp2;
            temp2 = swap;

            count1 = count1 << 1;
            count2 = count2 >> 1;
        }
    }

    /* main DCT-IV twiddles */
    {
        unsigned int count1 = half;
        unsigned int count2 = 1;
        float* temp1 = &ch->temp[0];
        float* temp2 = &ch->spectra[subframe][0];

        for (i = 0; i < mdct_bits; i++) {
            const float* sin_table = (const float*) sin_tables_hex[i];//todo cleanup
            const float* cos_table = (const float*) cos_tables_hex[i];
            float* swap;
            float* d1 = &temp2[0];
            float* d2 = &temp2[count2 * 2 - 1];
            const float* s1 = &temp1[0];
            const float* s2 = &temp1[count2];

            for (j = 0; j < count1; j++) {
                for (k = 0; k < count2; k++) {
                    float a = *(s1++);
                    float b = *(s2++);
                    float sin = *(sin_table++);
                    float cos = *(cos_table++);
                    *(d1++) = a * sin - b * cos;
                    *(d2--) = a * cos + b * sin;
                }
                s1 += count2;
                s2 += count2;
                d1 += count2;
                d2 += count2 * 3;
            }
            swap = temp1;
            temp1 = temp2;
            temp2 = swap;

            count1 = count1 >> 1;
            count2 = count2 << 1;
        }
#if 0
        /* copy dct */
        /* (with the above optimization spectra is already modified, so this is redundant) */
        for (i = 0; i < size; i++) {
            ch->dct[i] = ch->spectra[subframe][i];
        }
#endif
    }

    /* update output/imdct with overlapped window (lib fuses this with the above) */
    {
        const float* dct = &ch->spectra[subframe][0]; //ch->dct;
        const float* prev = &ch->imdct_previous[0];

        for (i = 0; i < half; i++) {
            ch->wave[subframe][i] = hcaimdct_window_float[i] * dct[i + half] + prev[i];
            ch->wave[subframe][i + half] = hcaimdct_window_float[i + half] * dct[size - 1 - i] - prev[i + half];
            ch->imdct_previous[i] = hcaimdct_window_float[size - 1 - i] * dct[half - i - 1];
            ch->imdct_previous[i + half] = hcaimdct_window_float[half - i - 1] * dct[i];
        }
#if 0
        /* over-optimized IMDCT window (for reference), barely noticeable even when decoding hundred of files */
        const float* imdct_window = hcaimdct_window_float;
        const float* dct;
        float* imdct_previous;
        float* wave = ch->wave[subframe];

        dct = &ch->dct[half];
        imdct_previous = ch->imdct_previous;
        for (i = 0; i < half; i++) {
            *(wave++) = *(dct++) * *(imdct_window++) + *(imdct_previous++);
        }
        for (i = 0; i < half; i++) {
            *(wave++) = *(imdct_window++) * *(--dct) - *(imdct_previous++);
        }
        /* implicit: imdct_window pointer is now at end */
        dct = &ch->dct[half - 1];
        imdct_previous = ch->imdct_previous;
        for (i = 0; i < half; i++) {
            *(imdct_previous++) = *(--imdct_window) * *(dct--);
        }
        for (i = 0; i < half; i++) {
            *(imdct_previous++) = *(--imdct_window) * *(++dct) ;
        }
#endif
    }
}
