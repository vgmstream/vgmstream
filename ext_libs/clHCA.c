/**
 * clHCA DECODER
 *
 * - Original decompilation and C++ decoder by nyaga
 *     https://github.com/Nyagamon/HCADecoder
 * - Ported to C by kode54
 *     https://gist.github.com/kode54/ce2bf799b445002e125f06ed833903c0
 * - Cleaned up by bnnm using Thealexbarney's VGAudio decoder as reference
 *     https://github.com/Thealexbarney/VGAudio
 */

/* TODO:
 * - improve portability on types and float casts, sizeof(int) isn't necessarily sizeof(float)
 * - check "packed_noise_level" vs VGAudio (CriHcaPacking.UnpackFrameHeader), weird behaviour
 * - check "delta scalefactors" vs VGAudio (CriHcaPacking.DeltaDecode), may be setting wrong values on bad data
 * - check "read intensity" vs VGAudio (CriHcaPacking.ReadIntensity), skips intensities if first is 15
 * - simplify DCT4 code
 * - add extra validations: encoder_delay/padding < sample_count, bands/totals (max: 128?), track count==1, etc
 * - calling clHCA_clear multiple times will not deallocate "comment" correctly
 */
//--------------------------------------------------
// Includes
//--------------------------------------------------
#include "clHCA.h"
#include <stddef.h>
#include <stdlib.h>
#include <memory.h>

#define HCA_MASK  0x7F7F7F7F /* chunk obfuscation when the HCA is encrypted with key */
#define HCA_SUBFRAMES_PER_FRAME  8
#define HCA_SAMPLES_PER_SUBFRAME  128
#define HCA_SAMPLES_PER_FRAME  (HCA_SUBFRAMES_PER_FRAME*HCA_SAMPLES_PER_SUBFRAME)
#define HCA_MDCT_BITS  7 /* (1<<7) = 128 */

#define HCA_MAX_CHANNELS  16 /* internal max? in practice only 8 can be encoded */

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
typedef struct stChannel {
    /* HCA channel config */
    int type; /* discrete / stereo-primary / stereo-secondary */
    unsigned int coded_scalefactor_count; /* scalefactors used (depending on channel type) */
    unsigned char *hfr_scales; /* high frequency scales, pointing to higher scalefactors (simplification) */

    /* subframe state */
    unsigned char intensity[HCA_SUBFRAMES_PER_FRAME];       /* intensity indexes (value max: 15 / 4b) */
    unsigned char scalefactors[HCA_SAMPLES_PER_SUBFRAME];   /* scale indexes (value max: 64 / 6b)*/
    unsigned char resolution[HCA_SAMPLES_PER_SUBFRAME];     /* resolution indexes (value max: 15 / 4b) */

    float gain[HCA_SAMPLES_PER_SUBFRAME];                   /* gain to apply to quantized spectral data */
    float spectra[HCA_SAMPLES_PER_SUBFRAME];                /* resulting dequantized data */
    float temp[HCA_SAMPLES_PER_SUBFRAME];                   /* temp for DCT-IV */
    float dct[HCA_SAMPLES_PER_SUBFRAME];                    /* result of DCT-IV */
    float imdct_previous[HCA_SAMPLES_PER_SUBFRAME];         /* IMDCT */

    /* frame state */
    float wave[HCA_SUBFRAMES_PER_FRAME][HCA_SAMPLES_PER_SUBFRAME];  /* resulting samples */
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
    unsigned int reserved1;
    unsigned int reserved2;
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
    unsigned int comment_len;
    char *comment;

    /* initial state */
    unsigned int hfr_group_count;
    unsigned char ath_curve[HCA_SAMPLES_PER_SUBFRAME];
    unsigned char cipher_table[256];
    /* variable state */
    stChannel channel[HCA_MAX_CHANNELS];
} clHCA;

typedef struct clData {
    const unsigned char *data;
    int size;
    int bit;
} clData; 


//--------------------------------------------------
// Checksum
//--------------------------------------------------
static const unsigned short crc16_lookup_table[256] = {
    0x0000,0x8005,0x800F,0x000A,0x801B,0x001E,0x0014,0x8011,0x8033,0x0036,0x003C,0x8039,0x0028,0x802D,0x8027,0x0022,
    0x8063,0x0066,0x006C,0x8069,0x0078,0x807D,0x8077,0x0072,0x0050,0x8055,0x805F,0x005A,0x804B,0x004E,0x0044,0x8041,
    0x80C3,0x00C6,0x00CC,0x80C9,0x00D8,0x80DD,0x80D7,0x00D2,0x00F0,0x80F5,0x80FF,0x00FA,0x80EB,0x00EE,0x00E4,0x80E1,
    0x00A0,0x80A5,0x80AF,0x00AA,0x80BB,0x00BE,0x00B4,0x80B1,0x8093,0x0096,0x009C,0x8099,0x0088,0x808D,0x8087,0x0082,
    0x8183,0x0186,0x018C,0x8189,0x0198,0x819D,0x8197,0x0192,0x01B0,0x81B5,0x81BF,0x01BA,0x81AB,0x01AE,0x01A4,0x81A1,
    0x01E0,0x81E5,0x81EF,0x01EA,0x81FB,0x01FE,0x01F4,0x81F1,0x81D3,0x01D6,0x01DC,0x81D9,0x01C8,0x81CD,0x81C7,0x01C2,
    0x0140,0x8145,0x814F,0x014A,0x815B,0x015E,0x0154,0x8151,0x8173,0x0176,0x017C,0x8179,0x0168,0x816D,0x8167,0x0162,
    0x8123,0x0126,0x012C,0x8129,0x0138,0x813D,0x8137,0x0132,0x0110,0x8115,0x811F,0x011A,0x810B,0x010E,0x0104,0x8101,
    0x8303,0x0306,0x030C,0x8309,0x0318,0x831D,0x8317,0x0312,0x0330,0x8335,0x833F,0x033A,0x832B,0x032E,0x0324,0x8321,
    0x0360,0x8365,0x836F,0x036A,0x837B,0x037E,0x0374,0x8371,0x8353,0x0356,0x035C,0x8359,0x0348,0x834D,0x8347,0x0342,
    0x03C0,0x83C5,0x83CF,0x03CA,0x83DB,0x03DE,0x03D4,0x83D1,0x83F3,0x03F6,0x03FC,0x83F9,0x03E8,0x83ED,0x83E7,0x03E2,
    0x83A3,0x03A6,0x03AC,0x83A9,0x03B8,0x83BD,0x83B7,0x03B2,0x0390,0x8395,0x839F,0x039A,0x838B,0x038E,0x0384,0x8381,
    0x0280,0x8285,0x828F,0x028A,0x829B,0x029E,0x0294,0x8291,0x82B3,0x02B6,0x02BC,0x82B9,0x02A8,0x82AD,0x82A7,0x02A2,
    0x82E3,0x02E6,0x02EC,0x82E9,0x02F8,0x82FD,0x82F7,0x02F2,0x02D0,0x82D5,0x82DF,0x02DA,0x82CB,0x02CE,0x02C4,0x82C1,
    0x8243,0x0246,0x024C,0x8249,0x0258,0x825D,0x8257,0x0252,0x0270,0x8275,0x827F,0x027A,0x826B,0x026E,0x0264,0x8261,
    0x0220,0x8225,0x822F,0x022A,0x823B,0x023E,0x0234,0x8231,0x8213,0x0216,0x021C,0x8219,0x0208,0x820D,0x8207,0x0202,
};

static unsigned short crc16_checksum(const unsigned char *data, unsigned int size) {
    unsigned int i;
    unsigned short sum = 0;

    /* HCA header/frames should always have checksum 0 (checksum(size-16b) = last 16b) */
    for (i = 0; i < size; i++) {
        sum = (sum << 8) ^ crc16_lookup_table[(sum >> 8) ^ data[i]];
    }
    return sum;
}

//--------------------------------------------------
// Bitstream reader
//--------------------------------------------------
static void bitreader_init(clData *br, const void *data, int size) {
    br->data = data;
    br->size = size * 8;
    br->bit = 0;
}

static unsigned int bitreader_peek(clData *br, int bitsize) {
    const unsigned int bit = br->bit;
    const unsigned int bit_rem = bit & 7;
    const unsigned int size = br->size;
    unsigned int v = 0;
    unsigned int bit_offset, bit_left;

    if (!(bit + bitsize <= size))
        return v;

    bit_offset = bitsize + bit_rem;
    bit_left = size - bit;
    if (bit_left >= 32 && bit_offset >= 25) {
        static const unsigned int mask[8] = {
                0xFFFFFFFF,0x7FFFFFFF,0x3FFFFFFF,0x1FFFFFFF,
                0x0FFFFFFF,0x07FFFFFF,0x03FFFFFF,0x01FFFFFF
        };
        const unsigned char *data = &br->data[bit >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v = (v << 8) | data[2];
        v = (v << 8) | data[3];
        v &= mask[bit_rem];
        v >>= 32 - bit_rem - bitsize;
    }
    else if (bit_left >= 24 && bit_offset >= 17) {
        static const unsigned int mask[8] = {
                0xFFFFFF,0x7FFFFF,0x3FFFFF,0x1FFFFF,
                0x0FFFFF,0x07FFFF,0x03FFFF,0x01FFFF
        };
        const unsigned char *data = &br->data[bit >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v = (v << 8) | data[2];
        v &= mask[bit_rem];
        v >>= 24 - bit_rem - bitsize;
    }
    else if (bit_left >= 16 && bit_offset >= 9) {
        static const unsigned int mask[8] = {
                0xFFFF,0x7FFF,0x3FFF,0x1FFF,0x0FFF,0x07FF,0x03FF,0x01FF
        };
        const unsigned char *data = &br->data[bit >> 3];
        v = data[0];
        v = (v << 8) | data[1];
        v &= mask[bit_rem];
        v >>= 16 - bit_rem - bitsize;
    }
    else {
        static const unsigned int mask[8] = {
                0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01
        };
        const unsigned char *data = &br->data[bit >> 3];
        v = data[0];
        v &= mask[bit_rem];
        v >>= 8 - bit_rem - bitsize;
    }
    return v;
}

static unsigned int bitreader_read(clData *br, int bitsize) {
    unsigned int v = bitreader_peek(br, bitsize);
    br->bit += bitsize;
    return v;
}

static void bitreader_skip(clData *br, int bitsize) {
    br->bit += bitsize;
}

//--------------------------------------------------
// API/Utilities
//--------------------------------------------------

int clHCA_isOurFile(const void *data, unsigned int size) {
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

int clHCA_getInfo(clHCA *hca, clHCA_stInfo *info) {
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
    return 0;
}

void clHCA_ReadSamples16(clHCA *hca, signed short *samples) {
    const float scale = 32768.0f;
    float f;
    signed int s;
    unsigned int i, j, k;

    for (i = 0; i < HCA_SUBFRAMES_PER_FRAME; i++) {
        for (j = 0; j < HCA_SAMPLES_PER_SUBFRAME; j++) {
            for (k = 0; k < hca->channels; k++) {
                f = hca->channel[k].wave[i][j];
                //f = f * hca->rva_volume; /* rare, won't apply for now */
                if (f > 1.0f) {
                    f = 1.0f;
                } else if (f < -1.0f) {
                    f = -1.0f;
                }
                s = (signed int) (f * scale);
                if ((unsigned) (s + 0x8000) & 0xFFFF0000)
                    s = (s >> 31) ^ 0x7FFF;
                *samples++ = (signed short) s;
            }
        }
    }
}


//--------------------------------------------------
// Allocation and creation
//--------------------------------------------------
static void clHCA_constructor(clHCA *hca) {
    if (!hca)
        return;
    memset(hca, 0, sizeof(*hca));
    hca->is_valid = 0;
    hca->comment = 0;
}

static void clHCA_destructor(clHCA *hca) {
    if (!hca)
        return;
    free(hca->comment);
    hca->comment = 0;
}

int clHCA_sizeof() {
    return sizeof(clHCA);
}

void clHCA_clear(clHCA *hca) {
    clHCA_constructor(hca);
}

void clHCA_done(clHCA *hca) {
    clHCA_destructor(hca);
}

clHCA * clHCA_new() {
    clHCA *hca = (clHCA *) malloc(clHCA_sizeof());
    if (hca) {
        clHCA_constructor(hca);
    }
    return hca;
}

void clHCA_delete(clHCA *hca) {
    clHCA_destructor(hca);
    free(hca);
}

//--------------------------------------------------
// ATH
//--------------------------------------------------
/* Base ATH (Absolute Threshold of Hearing) curve (for 41856hz).
 * May be a slight modification of the standard Painter & Spanias ATH curve formula. */
static const unsigned char ath_base_curve[656] = {
    0x78,0x5F,0x56,0x51,0x4E,0x4C,0x4B,0x49,0x48,0x48,0x47,0x46,0x46,0x45,0x45,0x45,
    0x44,0x44,0x44,0x44,0x43,0x43,0x43,0x43,0x43,0x43,0x42,0x42,0x42,0x42,0x42,0x42,
    0x42,0x42,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x40,0x40,0x40,0x40,
    0x40,0x40,0x40,0x40,0x40,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    0x3F,0x3F,0x3F,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,
    0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,
    0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,
    0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,
    0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3D,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3E,0x3F,
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    0x3F,0x3F,0x3F,0x3F,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
    0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
    0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x43,0x43,0x43,
    0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x44,0x44,
    0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x45,0x45,0x45,0x45,
    0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46,
    0x46,0x46,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x48,0x48,0x48,0x48,
    0x48,0x48,0x48,0x48,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x4A,0x4A,0x4A,0x4A,
    0x4A,0x4A,0x4A,0x4A,0x4B,0x4B,0x4B,0x4B,0x4B,0x4B,0x4B,0x4C,0x4C,0x4C,0x4C,0x4C,
    0x4C,0x4D,0x4D,0x4D,0x4D,0x4D,0x4D,0x4E,0x4E,0x4E,0x4E,0x4E,0x4E,0x4F,0x4F,0x4F,
    0x4F,0x4F,0x4F,0x50,0x50,0x50,0x50,0x50,0x51,0x51,0x51,0x51,0x51,0x52,0x52,0x52,
    0x52,0x52,0x53,0x53,0x53,0x53,0x54,0x54,0x54,0x54,0x54,0x55,0x55,0x55,0x55,0x56,
    0x56,0x56,0x56,0x57,0x57,0x57,0x57,0x57,0x58,0x58,0x58,0x59,0x59,0x59,0x59,0x5A,
    0x5A,0x5A,0x5A,0x5B,0x5B,0x5B,0x5B,0x5C,0x5C,0x5C,0x5D,0x5D,0x5D,0x5D,0x5E,0x5E,
    0x5E,0x5F,0x5F,0x5F,0x60,0x60,0x60,0x61,0x61,0x61,0x61,0x62,0x62,0x62,0x63,0x63,
    0x63,0x64,0x64,0x64,0x65,0x65,0x66,0x66,0x66,0x67,0x67,0x67,0x68,0x68,0x68,0x69,
    0x69,0x6A,0x6A,0x6A,0x6B,0x6B,0x6B,0x6C,0x6C,0x6D,0x6D,0x6D,0x6E,0x6E,0x6F,0x6F,
    0x70,0x70,0x70,0x71,0x71,0x72,0x72,0x73,0x73,0x73,0x74,0x74,0x75,0x75,0x76,0x76,
    0x77,0x77,0x78,0x78,0x78,0x79,0x79,0x7A,0x7A,0x7B,0x7B,0x7C,0x7C,0x7D,0x7D,0x7E,
    0x7E,0x7F,0x7F,0x80,0x80,0x81,0x81,0x82,0x83,0x83,0x84,0x84,0x85,0x85,0x86,0x86,
    0x87,0x88,0x88,0x89,0x89,0x8A,0x8A,0x8B,0x8C,0x8C,0x8D,0x8D,0x8E,0x8F,0x8F,0x90,
    0x90,0x91,0x92,0x92,0x93,0x94,0x94,0x95,0x95,0x96,0x97,0x97,0x98,0x99,0x99,0x9A,
    0x9B,0x9B,0x9C,0x9D,0x9D,0x9E,0x9F,0xA0,0xA0,0xA1,0xA2,0xA2,0xA3,0xA4,0xA5,0xA5,
    0xA6,0xA7,0xA7,0xA8,0xA9,0xAA,0xAA,0xAB,0xAC,0xAD,0xAE,0xAE,0xAF,0xB0,0xB1,0xB1,
    0xB2,0xB3,0xB4,0xB5,0xB6,0xB6,0xB7,0xB8,0xB9,0xBA,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xC9,0xCA,0xCB,0xCC,0xCD,
    0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,
    0xDE,0xDF,0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xED,0xEE,
    0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFF,0xFF,
};

static void ath_init0(unsigned char *ath_curve) {
    /* disable curve */
    memset(ath_curve, 0, sizeof(ath_curve[0]) * HCA_SAMPLES_PER_SUBFRAME);
}

static void ath_init1(unsigned char *ath_curve, unsigned int sample_rate) {
    unsigned int i, index;
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

static int ath_init(unsigned char *ath_curve, int type, unsigned int sample_rate) {
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
    return 0;
}


//--------------------------------------------------
// Encryption
//--------------------------------------------------
static void cipher_decrypt(unsigned char *cipher_table, unsigned char *data, int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        data[i] = cipher_table[data[i]];
    }
}

static void cipher_init0(unsigned char *cipher_table) {
    unsigned int i;

    /* no encryption */
    for (i = 0; i < 256; i++) {
        cipher_table[i] = i;
    }
}

static void cipher_init1(unsigned char *cipher_table) {
    const int mul = 13;
    const int add = 11;
    unsigned int i, v = 0;

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

static void cipher_init56_create_table(unsigned char *r, unsigned char key) {
    const int mul = ((key & 1) << 3) | 5;
    const int add = (key & 0xE) | 1;
    unsigned int i;

    key >>= 4;
    for (i = 0; i < 16; i++) {
        key = (key * mul + add) & 0xF;
        r[i] = key;
    }
}

static void cipher_init56(unsigned char *cipher_table, unsigned long long keycode) {
    unsigned char kc[8];
    unsigned char seed[16];
    unsigned char base[256], base_r[16], base_c[16];
    unsigned int r, c;

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
        unsigned int i;
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

static int cipher_init(unsigned char *cipher_table, int type, unsigned long long keycode) {
    if (type == 56 && !(keycode))
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
    return 0;
}

//--------------------------------------------------
// Parse
//--------------------------------------------------
static unsigned int header_ceil2(unsigned int a, unsigned int b) {
    return (b > 0) ? (a / b + ((a % b) ? 1 : 0)) : 0;
}

int clHCA_DecodeHeader(clHCA *hca, const void *data, unsigned int size) {
    clData br;
    int res;

    if (!hca || !data)
        return HCA_ERROR_PARAMS;

    hca->is_valid = 0;

    if (size < 0x08)
        return HCA_ERROR_PARAMS;

    bitreader_init(&br, data, size);

    /* read header chunks */

    /* HCA base header */
    if ((bitreader_peek(&br, 32) & HCA_MASK) == 0x48434100) { /* "HCA\0" */
        bitreader_skip(&br, 32);
        hca->version = bitreader_read(&br, 16);
        hca->header_size = bitreader_read(&br, 16);

#if 0   // play unknown versions anyway (confirmed to exist: v1.1/v1.2/v1.3/v2.0)
        if (hca->version != 0x0101 &&
                hca->version != 0x0102 &&
                hca->version != 0x0103 &&
                hca->version != 0x0200)
            return HCA_ERROR_HEADER;
#endif
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

        if (!(hca->channels >= 1 && hca->channels <= HCA_MAX_CHANNELS))
            return HCA_ERROR_HEADER;

        if (hca->frame_count == 0)
            return HCA_ERROR_HEADER;

        if (!(hca->sample_rate >= 1 && hca->sample_rate <= 0x7FFFFF)) /* encoder max seems 48000 */
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
        hca->reserved1 = bitreader_read(&br, 8);
        hca->reserved2 = bitreader_read(&br, 8);

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
        /* removed in v2.0, default in v1.x (maybe only used in v1.1, as v1.2/v1.3 set ath_type = 0) */
        hca->ath_type = (hca->version < 0x200) ? 1 : 0;
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
        hca->rva_volume = 1;
    }

    /* comment */
    if (size >= 0x05 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x636F6D6D) {/* "comm" */
        unsigned int i;
        char *temp;
        bitreader_skip(&br, 32);
        hca->comment_len = bitreader_read(&br, 8);

        if (hca->comment_len > size)
            return HCA_ERROR_HEADER;

        temp = realloc(hca->comment, hca->comment_len + 1);
        if (!temp)
            return HCA_ERROR_HEADER;
        hca->comment = temp;
        for (i = 0; i < hca->comment_len; ++i)
            hca->comment[i] = bitreader_read(&br, 8);
        hca->comment[i] = '\0'; /* should be null terminated but make sure */

        size -= 0x05 + hca->comment_len;
    }
    else {
        hca->comment_len = 0;
        hca->comment = NULL;
    }

    /* padding info */
    if (size >= 0x04 && (bitreader_peek(&br, 32) & HCA_MASK) == 0x70616400) { /* "pad\0" */
        size -= (size - 0x02); /* fills up to header_size, sans checksum */
    }

    /* should be fully read, but allow as data buffer may be bigger than header_size */
    //if (size != 0x02)
    //    return HCA_ERROR_HEADER;


    /* extra validations */
    if (!(hca->frame_size >= 0x08 && hca->frame_size <= 0xFFFF)) /* actual max seems 0x155*channels */
        return HCA_ERROR_HEADER; /* theoretically can be 0 if VBR (not seen) */

    if (!(hca->min_resolution == 1 && hca->max_resolution == 15))
        return HCA_ERROR_HEADER;


    /* inits state */
    if (hca->track_count == 0)
        hca->track_count = 1; /* default to avoid division by zero */

    hca->hfr_group_count = header_ceil2(
            hca->total_band_count - hca->base_band_count - hca->stereo_band_count,
            hca->bands_per_hfr_group);

    res = ath_init(hca->ath_curve, hca->ath_type, hca->sample_rate);
    if (res < 0)
        return res;
    res = cipher_init(hca->cipher_table, hca->ciph_type, hca->keycode);
    if (res < 0)
        return res;


    /* init channels */
    {
        int channel_types[HCA_MAX_CHANNELS] = {0};
        unsigned int i, channels_per_track;

        channels_per_track = hca->channels / hca->track_count;
        if (hca->stereo_band_count && channels_per_track > 1) {
            int *ct = channel_types;
            for (i = 0; i < hca->track_count; i++, ct += channels_per_track) {
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
                    ct[1] = 2;
                    if (hca->channel_config == 0) {
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
                    if (hca->channel_config <= 2) {
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
                }
            }
        }

        memset(hca->channel, 0, sizeof(hca->channel));
        for (i = 0; i < hca->channels; i++) {
            hca->channel[i].type = channel_types[i];
            hca->channel[i].coded_scalefactor_count = (channel_types[i] != 2) ?
                    hca->base_band_count + hca->stereo_band_count :
                    hca->base_band_count;
            hca->channel[i].hfr_scales = &hca->channel[i].scalefactors[hca->base_band_count + hca->stereo_band_count];
        }
    }


    /* clHCA is correctly initialized and decoder state reset
     * (keycode is not changed between calls) */
    hca->is_valid = 1;

    return 0;
}

void clHCA_SetKey(clHCA *hca, unsigned long long keycode) {
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

int clHCA_TestBlock(clHCA *hca, void *data, unsigned int size) {
    const float scale = 32768.0f;
    unsigned int ch, sf, s;
    int status;
    int clips = 0, blanks = 0;


    /* first blocks can be empty/silent, check all bytes but sync/crc */
    {
        int i;
        int is_empty = 1;
        const unsigned char *buf = data;

        for (i = 2; i < size - 0x02; i++) {
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
    status = clHCA_DecodeBlock(hca, data, size);
    if (status < 0)
        return -1;

    /* check decode results as bad keys may still get here */
    for (ch = 0; ch < hca->channels; ch++) {
        for (sf = 0; sf < HCA_SUBFRAMES_PER_FRAME; sf++) {
            for (s = 0; s < HCA_SAMPLES_PER_SUBFRAME; s++) {
                float fsample = hca->channel[ch].wave[sf][s];

                if (fsample > 1.0f || fsample < -1.0f) { //improve?
                    clips++;
                }
                else {
                    signed int psample = (signed int) (fsample * scale);
                    if (psample == 0 || psample == -1)
                        blanks++;
                }
            }
        }
    }

    /* the more clips the less likely block was correctly decrypted */
    if (clips == 1)
        clips++;
    if (clips > 1)
        return clips;
    /* if block is silent result is not useful */
    if (blanks == hca->channels * HCA_SUBFRAMES_PER_FRAME * HCA_SAMPLES_PER_SUBFRAME)
        return 0;

    /* block may be correct (but wrong keys can get this too and should test more blocks) */
    return 1;
}

void clHCA_DecodeReset(clHCA * hca) {
    unsigned int i;

    if (!hca || !hca->is_valid)
        return;

    for (i = 0; i < hca->channels; i++) {
        stChannel *ch = &hca->channel[i];

        /* most values get overwritten during decode */
        //memset(ch->intensity, 0, sizeof(ch->intensity[0]) * HCA_SUBFRAMES_PER_FRAME);
        //memset(ch->scalefactors, 0, sizeof(ch->scalefactors[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->resolution, 0, sizeof(ch->resolution[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->gain, 0, sizeof(ch->gain[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->spectra, 0, sizeof(ch->spectra[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->temp, 0, sizeof(ch->temp[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->dct, 0, sizeof(ch->dct[0]) * HCA_SAMPLES_PER_SUBFRAME);
        memset(ch->imdct_previous, 0, sizeof(ch->imdct_previous[0]) * HCA_SAMPLES_PER_SUBFRAME);
        //memset(ch->wave, 0, sizeof(ch->wave[0][0]) * HCA_SUBFRAMES_PER_FRAME * HCA_SUBFRAMES_PER_FRAME);
    }
}

//--------------------------------------------------
// Decode
//--------------------------------------------------
static int decode1_unpack_channel(stChannel *ch, clData *br,
        unsigned int hfr_group_count, unsigned int packed_noise_level, const unsigned char *ath_curve);

static void decode2_dequantize_coefficients(stChannel *ch, clData *br);

static void decode3_reconstruct_high_frequency(stChannel *ch,
        unsigned int hfr_group_count, unsigned int bands_per_hfr_group,
        unsigned int stereo_band_count, unsigned int base_band_count, unsigned int total_band_count);

static void decode4_apply_intensity_stereo(stChannel *ch, int subframe,
        unsigned int usable_band_count, unsigned int base_band_count, unsigned int stereo_band_count);

static void decoder5_run_imdct(stChannel *ch, int subframe);


int clHCA_DecodeBlock(clHCA *hca, void *data, unsigned int size) {
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
        unsigned int frame_acceptable_noise_level = bitreader_read(&br, 9);
        unsigned int frame_evaluation_boundary = bitreader_read(&br, 7);
        unsigned int packed_noise_level = (frame_acceptable_noise_level << 8) - frame_evaluation_boundary;

        for (ch = 0; ch < hca->channels; ch++) {
            int unpack = decode1_unpack_channel(&hca->channel[ch], &br,
                    hca->hfr_group_count, packed_noise_level, hca->ath_curve);
            if (unpack < 0)
                return unpack;
        }
    }

    for (subframe = 0; subframe < HCA_SUBFRAMES_PER_FRAME; subframe++) {

        /* unpack channel data and get dequantized spectra */
        for (ch = 0; ch < hca->channels; ch++){
            decode2_dequantize_coefficients(&hca->channel[ch], &br);
        }

        /* restore missing bands from spectra 1 */
        for (ch = 0; ch < hca->channels; ch++) {
            decode3_reconstruct_high_frequency(&hca->channel[ch],
                    hca->hfr_group_count, hca->bands_per_hfr_group,
                    hca->stereo_band_count, hca->base_band_count, hca->total_band_count);
        }

        /* restore missing bands from spectra 2 */
        for (ch = 0; ch < hca->channels - 1; ch++) {
            decode4_apply_intensity_stereo(&hca->channel[ch], subframe,
                    hca->total_band_count, hca->base_band_count, hca->stereo_band_count);
        }

        /* apply imdct */
        for (ch = 0; ch < hca->channels; ch++) {
            decoder5_run_imdct(&hca->channel[ch], subframe);
        }
    }

    /* should read all frame sans checksum at most */
    if (br.bit > br.size - 16)
        return HCA_ERROR_BITREADER;

    return 0;
}

//--------------------------------------------------
// Decode 1st step
//--------------------------------------------------
static const unsigned char decode1_scale_to_resolution_curve[64] = {
    0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0D,0x0D,
    0x0D,0x0D,0x0D,0x0D,0x0C,0x0C,0x0C,0x0C,
    0x0C,0x0C,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
    0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x09,
    0x09,0x09,0x09,0x09,0x09,0x08,0x08,0x08,
    0x08,0x08,0x08,0x07,0x06,0x06,0x05,0x04,
    0x04,0x04,0x03,0x03,0x03,0x02,0x02,0x02,
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* for v1.x indexes after 56 are different, but can't be used anyway */
  //0x02,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
};

/* scalefactor-to-scaling table, generated from sqrt(128) * (2^(53/128))^(scale_factor - 63) */
static const unsigned int decode1_dequantizer_scaling_table_int[64] = {
    0x342A8D26,0x34633F89,0x3497657D,0x34C9B9BE,0x35066491,0x353311C4,0x356E9910,0x359EF532,
    0x35D3CCF1,0x360D1ADF,0x363C034A,0x367A83B3,0x36A6E595,0x36DE60F5,0x371426FF,0x3745672A,
    0x37838359,0x37AF3B79,0x37E97C38,0x381B8D3A,0x384F4319,0x388A14D5,0x38B7FBF0,0x38F5257D,
    0x3923520F,0x39599D16,0x3990FA4D,0x39C12C4D,0x3A00B1ED,0x3A2B7A3A,0x3A647B6D,0x3A9837F0,
    0x3ACAD226,0x3B071F62,0x3B340AAF,0x3B6FE4BA,0x3B9FD228,0x3BD4F35B,0x3C0DDF04,0x3C3D08A4,
    0x3C7BDFED,0x3CA7CD94,0x3CDF9613,0x3D14F4F0,0x3D467991,0x3D843A29,0x3DB02F0E,0x3DEAC0C7,
    0x3E1C6573,0x3E506334,0x3E8AD4C6,0x3EB8FBAF,0x3EF67A41,0x3F243516,0x3F5ACB94,0x3F91C3D3,
    0x3FC238D2,0x400164D2,0x402C6897,0x4065B907,0x40990B88,0x40CBEC15,0x4107DB35,0x413504F3,
};
static const float *decode1_dequantizer_scaling_table = (const float *)decode1_dequantizer_scaling_table_int;

static const unsigned int decode1_quantizer_step_size_int[16] = {
    0x00000000,0x3F2AAAAB,0x3ECCCCCD,0x3E924925,0x3E638E39,0x3E3A2E8C,0x3E1D89D9,0x3E088889,
    0x3D842108,0x3D020821,0x3C810204,0x3C008081,0x3B804020,0x3B002008,0x3A801002,0x3A000801,
};
static const float *decode1_quantizer_step_size = (const float *)decode1_quantizer_step_size_int;

static int decode1_unpack_channel(stChannel *ch, clData *br,
        unsigned int hfr_group_count, unsigned int packed_noise_level, const unsigned char *ath_curve) {
    unsigned int i;
    const unsigned int csf_count = ch->coded_scalefactor_count;


    /* read scalefactors */
    {
        /* scale indexes to normalize dequantized coefficients */
        unsigned char scalefactor_delta_bits = bitreader_read(br, 3);
        if (scalefactor_delta_bits >= 6) {
            /* normal scalefactors */
            for (i = 0; i < csf_count; i++) {
                ch->scalefactors[i] = bitreader_read(br, 6);
            }
        }
        else if (scalefactor_delta_bits > 0) {
            /* delta scalefactors */
            const unsigned char expected_delta = (1 << scalefactor_delta_bits) - 1;
            const unsigned char extra_delta = expected_delta >> 1;
            unsigned char scalefactor_prev = bitreader_read(br, 6);

            ch->scalefactors[0] = scalefactor_prev;
            for (i = 1; i < csf_count; i++) {
                unsigned char delta = bitreader_read(br, scalefactor_delta_bits);

                if (delta != expected_delta) {
                    /* may happen with bad keycodes, scalefactors must be 6b indexes */
                    int scalefactor_test = (int)scalefactor_prev + ((int)delta - (int)extra_delta);
                    if (scalefactor_test < 0 || scalefactor_test > 64) {
                        return HCA_ERROR_UNPACK;
                    }

                    scalefactor_prev += delta - extra_delta;
                } else {
                    scalefactor_prev = bitreader_read(br, 6);
                }
                ch->scalefactors[i] = scalefactor_prev;
            }
        }
        else {
            /* no scalefactors */
            memset(ch->scalefactors, 0, sizeof(ch->scalefactors[0]) * HCA_SAMPLES_PER_SUBFRAME);
        }
    }

    if (ch->type == STEREO_SECONDARY) {
        /* read intensity */
        unsigned char intensity_value = bitreader_peek(br, 4);

        ch->intensity[0] = intensity_value;
        if (intensity_value < 15) {
            for (i = 0; i < HCA_SUBFRAMES_PER_FRAME; i++) {
                ch->intensity[i] = bitreader_read(br, 4);
            }
        }
        /* 15 may be an invalid value? */
        //else {
        //    return HCA_ERROR_INSENSITY;
        //}
    }
    else {
        /* read hfr scalefactors */
        for (i = 0; i < hfr_group_count; i++) {
            ch->hfr_scales[i] = bitreader_read(br, 6);
        }
    }

    /* calculate resolutions */
    {
        /* resolution determines the range of values per encoded spectra,
         * using codebooks for lower resolutions during dequantization */
        for (i = 0; i < csf_count; i++) {
            unsigned char new_resolution = 0;
            unsigned char scalefactor = ch->scalefactors[i];
            if (scalefactor > 0) {
                int noise_level = ath_curve[i] + ((packed_noise_level + i) >> 8);
                int curve_position = noise_level - ((5 * scalefactor) >> 1) + 1;

                /* curve values can be simplified by clamping position to (0,58) and making
                 * scale_table[0] = 15, table[58] = 1 (like VGAudio does) */
                if (curve_position < 0)
                    new_resolution = 15;
                else if (curve_position >= 57)
                    new_resolution = 1;
                else
                    new_resolution = decode1_scale_to_resolution_curve[curve_position];
            }
            ch->resolution[i] = new_resolution;
        }
        memset(&ch->resolution[csf_count], 0, sizeof(ch->resolution[0]) * (HCA_SAMPLES_PER_SUBFRAME - csf_count));
    }

    /* calculate gain */
    {
        /* get actual scales to dequantize */
        for (i = 0; i < csf_count; i++) {
            float scalefactor_scale = decode1_dequantizer_scaling_table[ ch->scalefactors[i] ];
            float resolution_scale = decode1_quantizer_step_size[ ch->resolution[i] ];
            ch->gain[i] = scalefactor_scale * resolution_scale;
        }
    }

    return 0;
}

//--------------------------------------------------
// Decode 2nd step
//--------------------------------------------------
static const unsigned char decode2_quantized_spectrum_max_bits[16] = {
    0,2,3,3,4,4,4,4,5,6,7,8,9,10,11,12
};
static const unsigned char decode2_quantized_spectrum_bits[128] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,2,2,0,0,0,0,0,0,0,0,0,0,0,0,
    2,2,2,2,2,2,3,3,0,0,0,0,0,0,0,0,
    2,2,3,3,3,3,3,3,0,0,0,0,0,0,0,0,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,
    3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,
    3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,
    3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
};
static const float decode2_quantized_spectrum_value[128] = {
    +0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,
    +0,+0,+1,-1,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,+0,
    +0,+0,+1,+1,-1,-1,+2,-2,+0,+0,+0,+0,+0,+0,+0,+0,
    +0,+0,+1,-1,+2,-2,+3,-3,+0,+0,+0,+0,+0,+0,+0,+0,
    +0,+0,+1,+1,-1,-1,+2,+2,-2,-2,+3,+3,-3,-3,+4,-4,
    +0,+0,+1,+1,-1,-1,+2,+2,-2,-2,+3,-3,+4,-4,+5,-5,
    +0,+0,+1,+1,-1,-1,+2,-2,+3,-3,+4,-4,+5,-5,+6,-6,
    +0,+0,+1,-1,+2,-2,+3,-3,+4,-4,+5,-5,+6,-6,+7,-7,
};

static void decode2_dequantize_coefficients(stChannel *ch, clData *br) {
    unsigned int i;
    const unsigned int csf_count = ch->coded_scalefactor_count;


    for (i = 0; i < csf_count; i++) {
        float qc;
        unsigned char resolution = ch->resolution[i];
        unsigned char bits = decode2_quantized_spectrum_max_bits[resolution];
        unsigned int code = bitreader_read(br, bits);

        /* read spectral coefficients */
        if (resolution < 8) {
            /* use prefix codebooks for lower resolutions */
            code += resolution << 4;
            bitreader_skip(br, decode2_quantized_spectrum_bits[code] - bits);
            qc = decode2_quantized_spectrum_value[code];
        }
        else {
            /* parse values in sign-magnitude form (lowest bit = sign) */
            int signed_code = (1 - ((code & 1) << 1)) * (code >> 1); /* move sign */
            if (signed_code == 0)
                bitreader_skip(br, -1); /* zero uses one less bit since it has no sign */
            qc = (float)signed_code;
        }

        /* dequantize coef with gain */
        ch->spectra[i] = ch->gain[i] * qc;
    }

    /* clean rest of spectra */
    memset(&ch->spectra[csf_count], 0, sizeof(ch->spectra[0]) * (HCA_SAMPLES_PER_SUBFRAME - csf_count));
}

//--------------------------------------------------
// Decode 3rd step
//--------------------------------------------------
static const unsigned int decode3_scale_conversion_table_int[128] = {
    0x00000000,0x00000000,0x32A0B051,0x32D61B5E,0x330EA43A,0x333E0F68,0x337D3E0C,0x33A8B6D5,
    0x33E0CCDF,0x3415C3FF,0x34478D75,0x3484F1F6,0x34B123F6,0x34EC0719,0x351D3EDA,0x355184DF,
    0x358B95C2,0x35B9FCD2,0x35F7D0DF,0x36251958,0x365BFBB8,0x36928E72,0x36C346CD,0x370218AF,
    0x372D583F,0x3766F85B,0x3799E046,0x37CD078C,0x3808980F,0x38360094,0x38728177,0x38A18FAF,
    0x38D744FD,0x390F6A81,0x393F179A,0x397E9E11,0x39A9A15B,0x39E2055B,0x3A16942D,0x3A48A2D8,
    0x3A85AAC3,0x3AB21A32,0x3AED4F30,0x3B1E196E,0x3B52A81E,0x3B8C57CA,0x3BBAFF5B,0x3BF9295A,
    0x3C25FED7,0x3C5D2D82,0x3C935A2B,0x3CC4563F,0x3D02CD87,0x3D2E4934,0x3D68396A,0x3D9AB62B,
    0x3DCE248C,0x3E0955EE,0x3E36FD92,0x3E73D290,0x3EA27043,0x3ED87039,0x3F1031DC,0x3F40213B,

    0x3F800000,0x3FAA8D26,0x3FE33F89,0x4017657D,0x4049B9BE,0x40866491,0x40B311C4,0x40EE9910,
    0x411EF532,0x4153CCF1,0x418D1ADF,0x41BC034A,0x41FA83B3,0x4226E595,0x425E60F5,0x429426FF,
    0x42C5672A,0x43038359,0x432F3B79,0x43697C38,0x439B8D3A,0x43CF4319,0x440A14D5,0x4437FBF0,
    0x4475257D,0x44A3520F,0x44D99D16,0x4510FA4D,0x45412C4D,0x4580B1ED,0x45AB7A3A,0x45E47B6D,
    0x461837F0,0x464AD226,0x46871F62,0x46B40AAF,0x46EFE4BA,0x471FD228,0x4754F35B,0x478DDF04,
    0x47BD08A4,0x47FBDFED,0x4827CD94,0x485F9613,0x4894F4F0,0x48C67991,0x49043A29,0x49302F0E,
    0x496AC0C7,0x499C6573,0x49D06334,0x4A0AD4C6,0x4A38FBAF,0x4A767A41,0x4AA43516,0x4ADACB94,
    0x4B11C3D3,0x4B4238D2,0x4B8164D2,0x4BAC6897,0x4BE5B907,0x4C190B88,0x4C4BEC15,0x00000000,
};
static const float *decode3_scale_conversion_table = (const float *)decode3_scale_conversion_table_int;

static void decode3_reconstruct_high_frequency(stChannel *ch,
        unsigned int hfr_group_count, unsigned int bands_per_hfr_group,
        unsigned int stereo_band_count, unsigned int base_band_count, unsigned int total_band_count) {
    if (ch->type == STEREO_SECONDARY)
        return;
    if (bands_per_hfr_group == 0) /* not in v1.x */
        return;

    {
        unsigned int group, i;
        unsigned int start_band = stereo_band_count + base_band_count;
        unsigned int highband = start_band;
        unsigned int lowband = start_band - 1;

        for (group = 0; group < hfr_group_count; group++) {
            for (i = 0; i < bands_per_hfr_group && highband < total_band_count; i++) {
                unsigned int sc_index = ch->hfr_scales[group] - ch->scalefactors[lowband] + 64;
                ch->spectra[highband] = decode3_scale_conversion_table[sc_index] * ch->spectra[lowband];
                highband++;
                lowband--;
            }
        }

        ch->spectra[HCA_SAMPLES_PER_SUBFRAME - 1] = 0; /* last spectral coefficient should be 0 */
    }
}

//--------------------------------------------------
// Decode 4th step
//--------------------------------------------------
static const unsigned int decode4_intensity_ratio_table_int[80] = {
    0x40000000,0x3FEDB6DB,0x3FDB6DB7,0x3FC92492,0x3FB6DB6E,0x3FA49249,0x3F924925,0x3F800000,
    0x3F5B6DB7,0x3F36DB6E,0x3F124925,0x3EDB6DB7,0x3E924925,0x3E124925,0x00000000,0x00000000,
    /* v2.0 seems to define indexes over 15, but intensity is packed in 4b thus unused */
    0x00000000,0x32A0B051,0x32D61B5E,0x330EA43A,0x333E0F68,0x337D3E0C,0x33A8B6D5,0x33E0CCDF,
    0x3415C3FF,0x34478D75,0x3484F1F6,0x34B123F6,0x34EC0719,0x351D3EDA,0x355184DF,0x358B95C2,
    0x35B9FCD2,0x35F7D0DF,0x36251958,0x365BFBB8,0x36928E72,0x36C346CD,0x370218AF,0x372D583F,
    0x3766F85B,0x3799E046,0x37CD078C,0x3808980F,0x38360094,0x38728177,0x38A18FAF,0x38D744FD,
    0x390F6A81,0x393F179A,0x397E9E11,0x39A9A15B,0x39E2055B,0x3A16942D,0x3A48A2D8,0x3A85AAC3,
    0x3AB21A32,0x3AED4F30,0x3B1E196E,0x3B52A81E,0x3B8C57CA,0x3BBAFF5B,0x3BF9295A,0x3C25FED7,
    0x3C5D2D82,0x3C935A2B,0x3CC4563F,0x3D02CD87,0x3D2E4934,0x3D68396A,0x3D9AB62B,0x3DCE248C,
    0x3E0955EE,0x3E36FD92,0x3E73D290,0x3EA27043,0x3ED87039,0x3F1031DC,0x3F40213B,0x00000000,
};
static const float *decode4_intensity_ratio_table = (const float *)decode4_intensity_ratio_table_int;

static void decode4_apply_intensity_stereo(stChannel *ch_pair, int subframe,
        unsigned int total_band_count, unsigned int base_band_count, unsigned int stereo_band_count) {
    if (ch_pair[0].type != STEREO_PRIMARY)
        return;
    if (stereo_band_count == 0)
        return;

    {
        float ratio_l = decode4_intensity_ratio_table[ ch_pair[1].intensity[subframe] ];
        float ratio_r = ratio_l - 2.0f;
        float *sp_l = ch_pair[0].spectra;
        float *sp_r = ch_pair[1].spectra;
        unsigned int band;

        for (band = base_band_count; band < total_band_count; band++) {
            sp_r[band] = sp_l[band] * ratio_r;
            sp_l[band] = sp_l[band] * ratio_l;
        }
    }
}

//--------------------------------------------------
// Decode 5th step
//--------------------------------------------------
static const unsigned int decode5_sin_tables_int[7][64] = {
    {
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
        0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,0x3DA73D75,
    },{
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
        0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,0x3F7B14BE,0x3F54DB31,
    },{
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
        0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,0x3F7EC46D,0x3F74FA0B,0x3F61C598,0x3F45E403,
    },{
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
        0x3F7FB10F,0x3F7D3AAC,0x3F7853F8,0x3F710908,0x3F676BD8,0x3F5B941A,0x3F4D9F02,0x3F3DAEF9,
    },{
        0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
        0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
        0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
        0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
        0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
        0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
        0x3F7FEC43,0x3F7F4E6D,0x3F7E1324,0x3F7C3B28,0x3F79C79D,0x3F76BA07,0x3F731447,0x3F6ED89E,
        0x3F6A09A7,0x3F64AA59,0x3F5EBE05,0x3F584853,0x3F514D3D,0x3F49D112,0x3F41D870,0x3F396842,
    },{
        0x3F7FFB11,0x3F7FD397,0x3F7F84AB,0x3F7F0E58,0x3F7E70B0,0x3F7DABCC,0x3F7CBFC9,0x3F7BACCD,
        0x3F7A7302,0x3F791298,0x3F778BC5,0x3F75DEC6,0x3F740BDD,0x3F721352,0x3F6FF573,0x3F6DB293,
        0x3F6B4B0C,0x3F68BF3C,0x3F660F88,0x3F633C5A,0x3F604621,0x3F5D2D53,0x3F59F26A,0x3F5695E5,
        0x3F531849,0x3F4F7A1F,0x3F4BBBF8,0x3F47DE65,0x3F43E200,0x3F3FC767,0x3F3B8F3B,0x3F373A23,
        0x3F7FFB11,0x3F7FD397,0x3F7F84AB,0x3F7F0E58,0x3F7E70B0,0x3F7DABCC,0x3F7CBFC9,0x3F7BACCD,
        0x3F7A7302,0x3F791298,0x3F778BC5,0x3F75DEC6,0x3F740BDD,0x3F721352,0x3F6FF573,0x3F6DB293,
        0x3F6B4B0C,0x3F68BF3C,0x3F660F88,0x3F633C5A,0x3F604621,0x3F5D2D53,0x3F59F26A,0x3F5695E5,
        0x3F531849,0x3F4F7A1F,0x3F4BBBF8,0x3F47DE65,0x3F43E200,0x3F3FC767,0x3F3B8F3B,0x3F373A23,
    },{
        0x3F7FFEC4,0x3F7FF4E6,0x3F7FE129,0x3F7FC38F,0x3F7F9C18,0x3F7F6AC7,0x3F7F2F9D,0x3F7EEA9D,
        0x3F7E9BC9,0x3F7E4323,0x3F7DE0B1,0x3F7D7474,0x3F7CFE73,0x3F7C7EB0,0x3F7BF531,0x3F7B61FC,
        0x3F7AC516,0x3F7A1E84,0x3F796E4E,0x3F78B47B,0x3F77F110,0x3F772417,0x3F764D97,0x3F756D97,
        0x3F748422,0x3F73913F,0x3F7294F8,0x3F718F57,0x3F708066,0x3F6F6830,0x3F6E46BE,0x3F6D1C1D,
        0x3F6BE858,0x3F6AAB7B,0x3F696591,0x3F6816A8,0x3F66BECC,0x3F655E0B,0x3F63F473,0x3F628210,
        0x3F6106F2,0x3F5F8327,0x3F5DF6BE,0x3F5C61C7,0x3F5AC450,0x3F591E6A,0x3F577026,0x3F55B993,
        0x3F53FAC3,0x3F5233C6,0x3F5064AF,0x3F4E8D90,0x3F4CAE79,0x3F4AC77F,0x3F48D8B3,0x3F46E22A,
        0x3F44E3F5,0x3F42DE29,0x3F40D0DA,0x3F3EBC1B,0x3F3CA003,0x3F3A7CA4,0x3F385216,0x3F36206C,
    }
};

static const unsigned int decode5_cos_tables_int[7][64]={
    {
        0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
        0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
        0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
        0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
        0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
        0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
        0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,
        0x3D0A8BD4,0xBD0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0xBD0A8BD4,0x3D0A8BD4,0x3D0A8BD4,0xBD0A8BD4,
    },{
        0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
        0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
        0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
        0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
        0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
        0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
        0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,
        0x3E47C5C2,0x3F0E39DA,0xBE47C5C2,0xBF0E39DA,0xBE47C5C2,0xBF0E39DA,0x3E47C5C2,0x3F0E39DA,
    },{
        0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
        0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
        0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
        0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
        0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
        0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
        0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,
        0x3DC8BD36,0x3E94A031,0x3EF15AEA,0x3F226799,0xBDC8BD36,0xBE94A031,0xBEF15AEA,0xBF226799,
    },{
        0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
        0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
        0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
        0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
        0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
        0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
        0xBD48FB30,0xBE164083,0xBE78CFCC,0xBEAC7CD4,0xBEDAE880,0xBF039C3D,0xBF187FC0,0xBF2BEB4A,
        0x3D48FB30,0x3E164083,0x3E78CFCC,0x3EAC7CD4,0x3EDAE880,0x3F039C3D,0x3F187FC0,0x3F2BEB4A,
    },{
        0xBCC90AB0,0xBD96A905,0xBDFAB273,0xBE2F10A2,0xBE605C13,0xBE888E93,0xBEA09AE5,0xBEB8442A,
        0xBECF7BCA,0xBEE63375,0xBEFC5D27,0xBF08F59B,0xBF13682A,0xBF1D7FD1,0xBF273656,0xBF3085BB,
        0x3CC90AB0,0x3D96A905,0x3DFAB273,0x3E2F10A2,0x3E605C13,0x3E888E93,0x3EA09AE5,0x3EB8442A,
        0x3ECF7BCA,0x3EE63375,0x3EFC5D27,0x3F08F59B,0x3F13682A,0x3F1D7FD1,0x3F273656,0x3F3085BB,
        0x3CC90AB0,0x3D96A905,0x3DFAB273,0x3E2F10A2,0x3E605C13,0x3E888E93,0x3EA09AE5,0x3EB8442A,
        0x3ECF7BCA,0x3EE63375,0x3EFC5D27,0x3F08F59B,0x3F13682A,0x3F1D7FD1,0x3F273656,0x3F3085BB,
        0xBCC90AB0,0xBD96A905,0xBDFAB273,0xBE2F10A2,0xBE605C13,0xBE888E93,0xBEA09AE5,0xBEB8442A,
        0xBECF7BCA,0xBEE63375,0xBEFC5D27,0xBF08F59B,0xBF13682A,0xBF1D7FD1,0xBF273656,0xBF3085BB,
    },{
        0xBC490E90,0xBD16C32C,0xBD7B2B74,0xBDAFB680,0xBDE1BC2E,0xBE09CF86,0xBE22ABB6,0xBE3B6ECF,
        0xBE541501,0xBE6C9A7F,0xBE827DC0,0xBE8E9A22,0xBE9AA086,0xBEA68F12,0xBEB263EF,0xBEBE1D4A,
        0xBEC9B953,0xBED53641,0xBEE0924F,0xBEEBCBBB,0xBEF6E0CB,0xBF00E7E4,0xBF064B82,0xBF0B9A6B,
        0xBF10D3CD,0xBF15F6D9,0xBF1B02C6,0xBF1FF6CB,0xBF24D225,0xBF299415,0xBF2E3BDE,0xBF32C8C9,
        0x3C490E90,0x3D16C32C,0x3D7B2B74,0x3DAFB680,0x3DE1BC2E,0x3E09CF86,0x3E22ABB6,0x3E3B6ECF,
        0x3E541501,0x3E6C9A7F,0x3E827DC0,0x3E8E9A22,0x3E9AA086,0x3EA68F12,0x3EB263EF,0x3EBE1D4A,
        0x3EC9B953,0x3ED53641,0x3EE0924F,0x3EEBCBBB,0x3EF6E0CB,0x3F00E7E4,0x3F064B82,0x3F0B9A6B,
        0x3F10D3CD,0x3F15F6D9,0x3F1B02C6,0x3F1FF6CB,0x3F24D225,0x3F299415,0x3F2E3BDE,0x3F32C8C9,
    },{
        0xBBC90F88,0xBC96C9B6,0xBCFB49BA,0xBD2FE007,0xBD621469,0xBD8A200A,0xBDA3308C,0xBDBC3AC3,
        0xBDD53DB9,0xBDEE3876,0xBE039502,0xBE1008B7,0xBE1C76DE,0xBE28DEFC,0xBE354098,0xBE419B37,
        0xBE4DEE60,0xBE5A3997,0xBE667C66,0xBE72B651,0xBE7EE6E1,0xBE8586CE,0xBE8B9507,0xBE919DDD,
        0xBE97A117,0xBE9D9E78,0xBEA395C5,0xBEA986C4,0xBEAF713A,0xBEB554EC,0xBEBB31A0,0xBEC1071E,
        0xBEC6D529,0xBECC9B8B,0xBED25A09,0xBED8106B,0xBEDDBE79,0xBEE363FA,0xBEE900B7,0xBEEE9479,
        0xBEF41F07,0xBEF9A02D,0xBEFF17B2,0xBF0242B1,0xBF04F484,0xBF07A136,0xBF0A48AD,0xBF0CEAD0,
        0xBF0F8784,0xBF121EB0,0xBF14B039,0xBF173C07,0xBF19C200,0xBF1C420C,0xBF1EBC12,0xBF212FF9,
        0xBF239DA9,0xBF26050A,0xBF286605,0xBF2AC082,0xBF2D1469,0xBF2F61A5,0xBF31A81D,0xBF33E7BC,
    }
};

/* HCA window function, close to a KBD window with an alpha of around 3.82 (similar to AAC/Vorbis) */
static const unsigned int decode5_imdct_window_int[128] = {
    0x3A3504F0,0x3B0183B8,0x3B70C538,0x3BBB9268,0x3C04A809,0x3C308200,0x3C61284C,0x3C8B3F17,
    0x3CA83992,0x3CC77FBD,0x3CE91110,0x3D0677CD,0x3D198FC4,0x3D2DD35C,0x3D434643,0x3D59ECC1,
    0x3D71CBA8,0x3D85741E,0x3D92A413,0x3DA078B4,0x3DAEF522,0x3DBE1C9E,0x3DCDF27B,0x3DDE7A1D,
    0x3DEFB6ED,0x3E00D62B,0x3E0A2EDA,0x3E13E72A,0x3E1E00B1,0x3E287CF2,0x3E335D55,0x3E3EA321,
    0x3E4A4F75,0x3E56633F,0x3E62DF37,0x3E6FC3D1,0x3E7D1138,0x3E8563A2,0x3E8C72B7,0x3E93B561,
    0x3E9B2AEF,0x3EA2D26F,0x3EAAAAAB,0x3EB2B222,0x3EBAE706,0x3EC34737,0x3ECBD03D,0x3ED47F46,
    0x3EDD5128,0x3EE6425C,0x3EEF4EFF,0x3EF872D7,0x3F00D4A9,0x3F0576CA,0x3F0A1D3B,0x3F0EC548,
    0x3F136C25,0x3F180EF2,0x3F1CAAC2,0x3F213CA2,0x3F25C1A5,0x3F2A36E7,0x3F2E9998,0x3F32E705,

    0xBF371C9E,0xBF3B37FE,0xBF3F36F2,0xBF431780,0xBF46D7E6,0xBF4A76A4,0xBF4DF27C,0xBF514A6F,
    0xBF547DC5,0xBF578C03,0xBF5A74EE,0xBF5D3887,0xBF5FD707,0xBF6250DA,0xBF64A699,0xBF66D908,
    0xBF68E90E,0xBF6AD7B1,0xBF6CA611,0xBF6E5562,0xBF6FE6E7,0xBF715BEF,0xBF72B5D1,0xBF73F5E6,
    0xBF751D89,0xBF762E13,0xBF7728D7,0xBF780F20,0xBF78E234,0xBF79A34C,0xBF7A5397,0xBF7AF439,
    0xBF7B8648,0xBF7C0ACE,0xBF7C82C8,0xBF7CEF26,0xBF7D50CB,0xBF7DA88E,0xBF7DF737,0xBF7E3D86,
    0xBF7E7C2A,0xBF7EB3CC,0xBF7EE507,0xBF7F106C,0xBF7F3683,0xBF7F57CA,0xBF7F74B6,0xBF7F8DB6,
    0xBF7FA32E,0xBF7FB57B,0xBF7FC4F6,0xBF7FD1ED,0xBF7FDCAD,0xBF7FE579,0xBF7FEC90,0xBF7FF22E,
    0xBF7FF688,0xBF7FF9D0,0xBF7FFC32,0xBF7FFDDA,0xBF7FFEED,0xBF7FFF8F,0xBF7FFFDF,0xBF7FFFFC,
};
static const float *decode5_imdct_window = (const float *)decode5_imdct_window_int;

static void decoder5_run_imdct(stChannel *ch, int subframe) {
    const static unsigned int size = HCA_SAMPLES_PER_SUBFRAME;
    const static unsigned int half = HCA_SAMPLES_PER_SUBFRAME / 2;
    const static unsigned int mdct_bits = HCA_MDCT_BITS;


    /* apply DCT-IV to dequantized spectra */
    {
        unsigned int i, j, k;
        unsigned int count1a, count2a, count1b, count2b;
        const float *temp1a, *temp1b;
        float *temp2a, *temp2b;

        /* this is all too crafty for me to simplify, see VGAudio (Mdct.Dct4) */

        temp1a = ch->spectra;
        temp2a = ch->temp;
        count1a = 1;
        count2a = half;
        for (i = 0; i < mdct_bits; i++) {
            float *swap;
            float *d1 = &temp2a[0];
            float *d2 = &temp2a[count2a];

            for (j = 0; j < count1a; j++) {
                for (k = 0; k < count2a; k++) {
                    float a = *(temp1a++);
                    float b = *(temp1a++);
                    *(d1++) = b + a;
                    *(d2++) = a - b;
                }
                d1 += count2a;
                d2 += count2a;
            }
            swap = (float*) temp1a - HCA_SAMPLES_PER_SUBFRAME; /* move spectra/temp to beginning */
            temp1a = temp2a;
            temp2a = swap;

            count1a = count1a << 1;
            count2a = count2a >> 1;
        }

        temp1b = ch->temp;
        temp2b = ch->spectra;
        count1b = half;
        count2b = 1;
        for (i = 0; i < mdct_bits; i++) {
            const float *sin_table = (const float *) decode5_sin_tables_int[i];
            const float *cos_table = (const float *) decode5_cos_tables_int[i];
            float *swap;
            float *d1 = temp2b;
            float *d2 = &temp2b[count2b * 2 - 1];
            const float *s1 = &temp1b[0];
            const float *s2 = &temp1b[count2b];

            for (j = 0; j < count1b; j++) {
                for (k = 0; k < count2b; k++) {
                    float a = *(s1++);
                    float b = *(s2++);
                    float sin = *(sin_table++);
                    float cos = *(cos_table++);
                    *(d1++) = a * sin - b * cos;
                    *(d2--) = a * cos + b * sin;
                }
                s1 += count2b;
                s2 += count2b;
                d1 += count2b;
                d2 += count2b * 3;
            }
            swap = (float*) temp1b;
            temp1b = temp2b;
            temp2b = swap;

            count1b = count1b >> 1;
            count2b = count2b << 1;
        }

        /* copy dct */
        /* (with the above optimization spectra is already modified, so this is redundant) */
        for (i = 0; i < size; i++) {
            ch->dct[i] = ch->spectra[i];
        }
    }

    /* update output/imdct */
    {
        unsigned int i;

        for (i = 0; i < half; i++) {
            ch->wave[subframe][i] = decode5_imdct_window[i] * ch->dct[i + half] + ch->imdct_previous[i];
            ch->wave[subframe][i + half] = decode5_imdct_window[i + half] * ch->dct[size - 1 - i] - ch->imdct_previous[i + half];
            ch->imdct_previous[i] = decode5_imdct_window[size - 1 - i] * ch->dct[half - i - 1];
            ch->imdct_previous[i + half] = decode5_imdct_window[half - i - 1] * ch->dct[i];
        }
#if 0
        /* over-optimized IMDCT (for reference), barely noticeable even when decoding hundred of files */
        const float *imdct_window = decode5_imdct_window;
        const float *dct;
        float *imdct_previous;
        float *wave = ch->wave[subframe];

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
