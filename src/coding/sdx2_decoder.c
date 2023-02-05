#include <math.h>
#include "coding.h"
#include "../util.h"

/* SDX2 - 2:1 Squareroot-delta-exact compression */
/* CBD2 - 2:1 Cuberoot-delta-exact compression (from 3DO/Konami M2) */

/* Original code is implemented with ops in a custom DSP (DSPP) with a variation of FORTH
 * (rather than tables), in the "3DO M2 Portfolio OS", so could be converted to plain calcs. */
 

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

/*
// original DSP ops, seems equivalent to (i * i * i) / 64
for (uint16_t i = 0; i < 0x100; i += 1) {
    int16_t v = i;
    v -= 0x80;
    v *= 0x100;
    int32_t a = v;
    a *= a;
    a >>= 15;
    a *= v;
    a >>= 15;
    cubes[i] = a;
}
*/
static int16_t cubes[256] = {
        -32768,-32006,-31256,-30518,-29791,-29077,-28373,-27681,-27000,-26331,-25673,
        -25026,-24389,-23764,-23150,-22546,-21952,-21370,-20797,-20235,-19683,-19142,
        -18610,-18088,-17576,-17074,-16582,-16099,-15625,-15161,-14707,-14261,-13824,
        -13397,-12978,-12569,-12167,-11775,-11391,-11016,-10648,-10290, -9939, -9596,
         -9261, -8935, -8616, -8304, -8000, -7704, -7415, -7134, -6859, -6592, -6332,
         -6079, -5832, -5593, -5360, -5133, -4913, -4700, -4493, -4292, -4096, -3907,
         -3724, -3547, -3375, -3210, -3049, -2894, -2744, -2600, -2461, -2327, -2197,
         -2073, -1954, -1839, -1728, -1623, -1521, -1424, -1331, -1243, -1158, -1077,
         -1000,  -927,  -858,  -792,  -729,  -670,  -615,  -562,  -512,  -466,  -422,
          -382,  -343,  -308,  -275,  -245,  -216,  -191,  -167,  -145,  -125,  -108,
           -92,   -77,   -64,   -53,   -43,   -35,   -27,   -21,   -16,   -12,    -8,
            -6,    -4,    -2,    -1,    -1,    -1,    -1,     0,     0,     0,     0,
             1,     1,     3,     5,     8,    11,    15,    20,    27,    34,    42,
            52,    64,    76,    91,   107,   125,   144,   166,   190,   216,   244,
           274,   307,   343,   381,   421,   465,   512,   561,   614,   669,   729,
           791,   857,   926,  1000,  1076,  1157,  1242,  1331,  1423,  1520,  1622,
          1728,  1838,  1953,  2072,  2197,  2326,  2460,  2599,  2744,  2893,  3048,
          3209,  3375,  3546,  3723,  3906,  4096,  4291,  4492,  4699,  4913,  5132,
          5359,  5592,  5832,  6078,  6331,  6591,  6859,  7133,  7414,  7703,  8000,
          8303,  8615,  8934,  9261,  9595,  9938, 10289, 10648, 11015, 11390, 11774,
         12167, 12568, 12977, 13396, 13824, 14260, 14706, 15160, 15625, 16098, 16581,
         17073, 17576, 18087, 18609, 19141, 19683, 20234, 20796, 21369, 21952, 22545,
         23149, 23763, 24389, 25025, 25672, 26330, 27000, 27680, 28372, 29076, 29791,
         30517, 31255, 32005
};

static void decode_delta_exact(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int16_t * table) {
    int32_t hist = stream->adpcm_history1_32;

    int i;
    int32_t sample_count = 0;

    for (i=first_sample; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int8_t sample_byte = read_8bit(stream->offset+i,stream->streamfile);
        int16_t sample;

        if (!(sample_byte & 1)) /* even: "exact mode" (value as-is), odd: "delta mode" */
            hist = 0;
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

        if (!(sample_byte & 1)) /* even: "exact mode" (value as-is), odd: "delta mode" */
            hist = 0;
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
