#include <math.h>
#include "coding.h"
#include "../util.h"

/* SDX2 - 2:1 Squareroot-delta-exact compression */
/* CBD2 - 2:1 Cuberoot-delta-exact compression (from the unreleased 3DO M2) */

/* for (i=-128;i<128;i++) { squares[i+128] = i<0?(-i*i)*2:(i*i)*2; } */
static int16_t squares[256] = {
        -32768,-32258,-31752,-31250,-30752,-30258,-29768,-29282,-28800,-28322,-27848,
        -27378,-26912,-26450,-25992,-25538,-25088,-24642,-24200,-23762,-23328,-22898,
        -22472,-22050,-21632,-21218,-20808,-20402,-20000,-19602,-19208,-18818,-18432,
        -18050,-17672,-17298,-16928,-16562,-16200,-15842,-15488,-15138,-14792,-14450,
        -14112,-13778,-13448,-13122,-12800,-12482,-12168,-11858,-11552,-11250,-10952,
        -10658,-10368,-10082, -9800, -9522, -9248, -8978, -8712, -8450, -8192, -7938,
         -7688, -7442, -7200, -6962, -6728, -6498, -6272, -6050, -5832, -5618, -5408,
         -5202, -5000, -4802, -4608, -4418, -4232, -4050, -3872, -3698, -3528, -3362,
         -3200, -3042, -2888, -2738, -2592, -2450, -2312, -2178, -2048, -1922, -1800,
         -1682, -1568, -1458, -1352, -1250, -1152, -1058,  -968,  -882,  -800,  -722,
          -648,  -578,  -512,  -450,  -392,  -338,  -288,  -242,  -200,  -162,  -128,
           -98,   -72,   -50,   -32,   -18,    -8,    -2,     0,     2,     8,    18,
            32,    50,    72,    98,   128,   162,   200,   242,   288,   338,   392,
           450,   512,   578,   648,   722,   800,   882,   968,  1058,  1152,  1250,
          1352,  1458,  1568,  1682,  1800,  1922,  2048,  2178,  2312,  2450,  2592,
          2738,  2888,  3042,  3200,  3362,  3528,  3698,  3872,  4050,  4232,  4418,
          4608,  4802,  5000,  5202,  5408,  5618,  5832,  6050,  6272,  6498,  6728,
          6962,  7200,  7442,  7688,  7938,  8192,  8450,  8712,  8978,  9248,  9522,
          9800, 10082, 10368, 10658, 10952, 11250, 11552, 11858, 12168, 12482, 12800,
         13122, 13448, 13778, 14112, 14450, 14792, 15138, 15488, 15842, 16200, 16562,
         16928, 17298, 17672, 18050, 18432, 18818, 19208, 19602, 20000, 20402, 20808,
         21218, 21632, 22050, 22472, 22898, 23328, 23762, 24200, 24642, 25088, 25538,
         25992, 26450, 26912, 27378, 27848, 28322, 28800, 29282, 29768, 30258, 30752,
         31250, 31752, 32258
};

/* for (i=-128;i<128;i++) { double j = (i/2)/2.0; cubes[i+128] = floor(j*j*j); } */
static int16_t cubes[256] = {
        -32768,-31256,-31256,-29791,-29791,-28373,-28373,-27000,-27000,-25672,-25672,
        -24389,-24389,-23149,-23149,-21952,-21952,-20797,-20797,-19683,-19683,-18610,
        -18610,-17576,-17576,-16581,-16581,-15625,-15625,-14706,-14706,-13824,-13824,
        -12978,-12978,-12167,-12167,-11391,-11391,-10648,-10648, -9938, -9938, -9261,
         -9261, -8615, -8615, -8000, -8000, -7415, -7415, -6859, -6859, -6332, -6332,
         -5832, -5832, -5359, -5359, -4913, -4913, -4492, -4492, -4096, -4096, -3724,
         -3724, -3375, -3375, -3049, -3049, -2744, -2744, -2460, -2460, -2197, -2197,
         -1953, -1953, -1728, -1728, -1521, -1521, -1331, -1331, -1158, -1158, -1000,
         -1000,  -857,  -857,  -729,  -729,  -614,  -614,  -512,  -512,  -422,  -422,
          -343,  -343,  -275,  -275,  -216,  -216,  -166,  -166,  -125,  -125,   -91,
           -91,   -64,   -64,   -43,   -43,   -27,   -27,   -16,   -16,    -8,    -8,
            -3,    -3,    -1,    -1,     0,     0,     0,     0,     0,     0,     0,
             1,     1,     3,     3,     8,     8,    16,    16,    27,    27,    43,
            43,    64,    64,    91,    91,   125,   125,   166,   166,   216,   216,
           275,   275,   343,   343,   422,   422,   512,   512,   614,   614,   729,
           729,   857,   857,  1000,  1000,  1158,  1158,  1331,  1331,  1521,  1521,
          1728,  1728,  1953,  1953,  2197,  2197,  2460,  2460,  2744,  2744,  3049,
          3049,  3375,  3375,  3724,  3724,  4096,  4096,  4492,  4492,  4913,  4913,
          5359,  5359,  5832,  5832,  6332,  6332,  6859,  6859,  7415,  7415,  8000,
          8000,  8615,  8615,  9261,  9261,  9938,  9938, 10648, 10648, 11391, 11391,
         12167, 12167, 12978, 12978, 13824, 13824, 14706, 14706, 15625, 15625, 16581,
         16581, 17576, 17576, 18610, 18610, 19683, 19683, 20797, 20797, 21952, 21952,
         23149, 23149, 24389, 24389, 25672, 25672, 27000, 27000, 28373, 28373, 29791,
         29791, 31256, 31256
};

static void decode_delta_exact(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int16_t * table) {
    int32_t hist = stream->adpcm_history1_32;

    int i;
    int32_t sample_count = 0;

    for (i=first_sample; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int8_t sample_byte = read_8bit(stream->offset+i,stream->streamfile);
        int16_t sample;

        if (!(sample_byte & 1)) hist = 0;
        sample = hist + table[sample_byte+128];

        hist = outbuf[sample_count] = clamp16(sample);
    }

    stream->adpcm_history1_32 = hist;
}

static void decode_delta_exact_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int16_t * table) {
    int32_t hist = stream->adpcm_history1_32;

    int i;
    int32_t sample_count = 0;

    for (i=first_sample; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int8_t sample_byte = read_8bit(stream->offset+i*channelspacing,stream->streamfile);
        int16_t sample;

        if (!(sample_byte & 1)) hist = 0;
        sample = hist + table[sample_byte+128];

        hist = outbuf[sample_count] = clamp16(sample);
    }

    stream->adpcm_history1_32 = hist;
}

void decode_sdx2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    decode_delta_exact(stream, outbuf, channelspacing, first_sample, samples_to_do, squares);
}

void decode_sdx2_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    decode_delta_exact_int(stream, outbuf, channelspacing, first_sample, samples_to_do, squares);
}

void decode_cbd2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    decode_delta_exact(stream, outbuf, channelspacing, first_sample, samples_to_do, cubes);
}

void decode_cbd2_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    decode_delta_exact_int(stream, outbuf, channelspacing, first_sample, samples_to_do, cubes);
}
