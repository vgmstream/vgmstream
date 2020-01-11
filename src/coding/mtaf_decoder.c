#include "coding.h"
#include "../util.h"


/* A hybrid of IMA and Yamaha ADPCM found in Metal Gear Solid 3
 * Thanks to X_Tra (http://metalgear.in/) for pointing me to the step size table.
 *
 * Layout: N tracks of 0x10 header + 0x80*2 (always 2ch; multichannels uses 4ch = 2ch track0 + 2ch track1 xN).
 */

static const int mtaf_step_indexes[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
}; 

static const int16_t mtaf_step_sizes[32][16] = {
    {     1,     5,     9,    13,    16,    20,    24,    28,
         -1,    -5,    -9,   -13,   -16,   -20,   -24,   -28, },
    {     2,     6,    11,    15,    20,    24,    29,    33,
         -2,    -6,   -11,   -15,   -20,   -24,   -29,   -33, },
    {     2,     7,    13,    18,    23,    28,    34,    39,
         -2,    -7,   -13,   -18,   -23,   -28,   -34,   -39, },
    {     3,     9,    15,    21,    28,    34,    40,    46,
         -3,    -9,   -15,   -21,   -28,   -34,   -40,   -46, },
    {     3,    11,    18,    26,    33,    41,    48,    56,
         -3,   -11,   -18,   -26,   -33,   -41,   -48,   -56, },
    {     4,    13,    22,    31,    40,    49,    58,    67,
         -4,   -13,   -22,   -31,   -40,   -49,   -58,   -67, },
    {     5,    16,    26,    37,    48,    59,    69,    80,
         -5,   -16,   -26,   -37,   -48,   -59,   -69,   -80, },
    {     6,    19,    31,    44,    57,    70,    82,    95,
         -6,   -19,   -31,   -44,   -57,   -70,   -82,   -95, },
    {     7,    22,    38,    53,    68,    83,    99,   114,
         -7,   -22,   -38,   -53,   -68,   -83,   -99,  -114, },
    {     9,    27,    45,    63,    81,    99,   117,   135,
         -9,   -27,   -45,   -63,   -81,   -99,  -117,  -135, },
    {    10,    32,    53,    75,    96,   118,   139,   161,
        -10,   -32,   -53,   -75,   -96,  -118,  -139,  -161, },
    {    12,    38,    64,    90,   115,   141,   167,   193,
        -12,   -38,   -64,   -90,  -115,  -141,  -167,  -193, },
    {    15,    45,    76,   106,   137,   167,   198,   228,
        -15,   -45,   -76,  -106,  -137,  -167,  -198,  -228, },
    {    18,    54,    91,   127,   164,   200,   237,   273,
        -18,   -54,   -91,  -127,  -164,  -200,  -237,  -273, },
    {    21,    65,   108,   152,   195,   239,   282,   326,
        -21,   -65,  -108,  -152,  -195,  -239,  -282,  -326, },
    {    25,    77,   129,   181,   232,   284,   336,   388,
        -25,   -77,  -129,  -181,  -232,  -284,  -336,  -388, },
    {    30,    92,   153,   215,   276,   338,   399,   461,
        -30,   -92,  -153,  -215,  -276,  -338,  -399,  -461, },
    {    36,   109,   183,   256,   329,   402,   476,   549,
        -36,  -109,  -183,  -256,  -329,  -402,  -476,  -549, },
    {    43,   130,   218,   305,   392,   479,   567,   654,
        -43,  -130,  -218,  -305,  -392,  -479,  -567,  -654, },
    {    52,   156,   260,   364,   468,   572,   676,   780,
        -52,  -156,  -260,  -364,  -468,  -572,  -676,  -780, },
    {    62,   186,   310,   434,   558,   682,   806,   930,
        -62,  -186,  -310,  -434,  -558,  -682,  -806,  -930, },
    {    73,   221,   368,   516,   663,   811,   958,  1106,
        -73,  -221,  -368,  -516,  -663,  -811,  -958, -1106, },
    {    87,   263,   439,   615,   790,   966,  1142,  1318,
        -87,  -263,  -439,  -615,  -790,  -966, -1142, -1318, },
    {   104,   314,   523,   733,   942,  1152,  1361,  1571,
       -104,  -314,  -523,  -733,  -942, -1152, -1361, -1571, },
    {   124,   374,   623,   873,  1122,  1372,  1621,  1871,
       -124,  -374,  -623,  -873, -1122, -1372, -1621, -1871, },
    {   148,   445,   743,  1040,  1337,  1634,  1932,  2229,
       -148,  -445,  -743, -1040, -1337, -1634, -1932, -2229, },
    {   177,   531,   885,  1239,  1593,  1947,  2301,  2655,
       -177,  -531,  -885, -1239, -1593, -1947, -2301, -2655, },
    {   210,   632,  1053,  1475,  1896,  2318,  2739,  3161,
       -210,  -632, -1053, -1475, -1896, -2318, -2739, -3161, },
    {   251,   753,  1255,  1757,  2260,  2762,  3264,  3766,
       -251,  -753, -1255, -1757, -2260, -2762, -3264, -3766, },
    {   299,   897,  1495,  2093,  2692,  3290,  3888,  4486,
       -299,  -897, -1495, -2093, -2692, -3290, -3888, -4486, },
    {   356,  1068,  1781,  2493,  3206,  3918,  4631,  5343,
       -356, -1068, -1781, -2493, -3206, -3918, -4631, -5343, },
    {   424,  1273,  2121,  2970,  3819,  4668,  5516,  6365,
       -424, -1273, -2121, -2970, -3819, -4668, -5516, -6365, },
};


void decode_mtaf(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    uint8_t frame[0x110] = {0};
    off_t frame_offset;
    int i, ch, sample_count = 0;
    size_t bytes_per_frame /*, samples_per_frame*/;
    int32_t hist = stream->adpcm_history1_16;
    int32_t step_index = stream->adpcm_step_index;


    /* special stereo interleave, stereo */
    bytes_per_frame = 0x10 + 0x80*2;
    //samples_per_frame = (bytes_per_frame - 0x10) / 2 * 2; /* 256 */
    ch = channel % 2; /* global channel to track channel */
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* read frame */
    frame_offset = stream->offset;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    /* parse frame header when we hit a new track every frame samples */
    if (first_sample == 0) {
        /*  0x10 header: track (8b, 0=first), track count (24b, 1=first), step-L, step-R, hist-L, hist-R */
        step_index = get_s16le(frame + 0x04 + 0x00 + ch*0x02); /* step-L/R */
        hist       = get_s16le(frame + 0x04 + 0x04 + ch*0x04); /* hist-L/R: hist 16bit + empty 16bit */

        VGM_ASSERT(step_index < 0 || step_index > 31, "MTAF: bad header idx at 0x%x\n", (uint32_t)stream->offset);
        if (step_index < 0) {
            step_index = 0;
        } else if (step_index > 31) {
            step_index = 31;
        }
    }

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x10 + 0x80*ch + i/2];
        uint8_t nibble = (nibbles >> (!(i&1)?0:4)) & 0xf; /* lower first */

        hist = clamp16(hist + mtaf_step_sizes[step_index][nibble]);
        outbuf[sample_count] = hist;
        sample_count += channelspacing;

        step_index += mtaf_step_indexes[nibble];
        if (step_index < 0) {
            step_index = 0;
        } else if (step_index > 31) {
            step_index = 31;
        }
    }

    stream->adpcm_step_index = step_index;
    stream->adpcm_history1_16 = hist;
}
