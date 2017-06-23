#include "coding.h"
#include "../util.h"

#define MTAF_BLOCK_SUPPORT


/* A hybrid of IMA and Yamaha ADPCM found in Metal Gear Solid 3
 * Thanks to X_Tra (http://metalgear.in/) for pointing me to the step size table.
 *
 * Layout: N tracks of 0x10 header + 0x80*2 (always 2ch; multichannels uses 4ch = 2ch track0 + 2ch track1 xN)
 * "macroblocks" support is not really needed as the extractors should remove them but they are
 * autodetected and skipped if found (ideally should keep a special layout/count, but this is simpler).
 */

static const int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
}; 

static const int16_t step_size[32][16] = {
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

#ifdef MTAF_BLOCK_SUPPORT
/* autodetect and skip "macroblocks" */
static void mtaf_block_update(VGMSTREAMCHANNEL * stream) {
    int block_type, block_size, block_empty, block_tracks, repeat = 1;

    do {
        block_type   = read_32bitLE(stream->offset+0x00, stream->streamfile);
        block_size   = read_32bitLE(stream->offset+0x04, stream->streamfile); /* including this header */
        block_empty  = read_32bitLE(stream->offset+0x08, stream->streamfile); /* always 0 */
        block_tracks = read_32bitLE(stream->offset+0x0c, stream->streamfile); /* total tracks of 0x110 (can be 0)*/

        /* 0x110001: music (type 0x11=adpcm), 0xf0: loop control (goes at the end) */
        if ((block_type != 0x00110001 && block_type != 0x000000F0) || block_empty != 0)
            return; /* not a block */

        /* track=001100+01 could be mistaken as block_type, do extra checks */
        {
            int track = read_8bit(stream->offset+0x10, stream->streamfile);
            if (track != 0 && track != 1)
                return; /* if this is a block, next header should be from track 0/1 */
            if (block_tracks > 0 && (block_size-0x10) != block_tracks*0x110)
                return; /* wrong expected size */
        }

        if (block_size <= 0 || block_tracks < 0) {  /* nonsense block (maybe at EOF) */
            VGM_LOG("MTAF: bad block @ %08lx\n", stream->offset);
            stream->offset += 0x10;
            repeat = 0;
        }
        else if (block_tracks == 0) {  /* empty block (common), keep repeating */
            stream->offset += block_size;
        }
        else {  /* normal block, position into next track header */
            stream->offset += 0x10;
            repeat = 0;
        }

    } while(repeat);
}
#endif

void decode_mtaf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int channels) {
    int32_t sample_count;
    int i;
    int c = channel%2; /* global channel to track channel */
    int32_t hist = stream->adpcm_history1_16;
    int32_t step_idx = stream->adpcm_step_index;


    #ifdef MTAF_BLOCK_SUPPORT
    /* autodetect and skip macroblock header */
    mtaf_block_update(stream);
    #endif

    /* read header when we hit a new track every 0x100 samples */
    first_sample = first_sample % 0x100;

    if (first_sample == 0) {
        /*  0x10 header: track (8b, 0=first), track count (24b, 1=first), step-L, step-R, hist-L, hist-R */
        int32_t init_idx  = read_16bitLE(stream->offset+4+0+c*2, stream->streamfile); /* step-L/R */
        int32_t init_hist = read_16bitLE(stream->offset+4+4+c*4, stream->streamfile); /* hist-L/R: hist 16bit + empty 16bit */

        VGM_ASSERT(init_idx < 0 || init_idx > 31, "MTAF: bad header idx @ 0x%lx\n", stream->offset);
        /* avoid index out of range in corrupt files */
        if (init_idx < 0) {
            init_idx = 0;
        } else if (init_idx > 31) {
            init_idx = 31;
        }

        step_idx = init_idx;
        hist = init_hist;
    }


    /* skip to nibble */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t byte = read_8bit(stream->offset + 0x10 + 0x80*c + i/2, stream->streamfile);
        uint8_t nibble = (byte >> (!(i&1)?0:4)) & 0xf; /* lower first */

        hist = clamp16(hist+step_size[step_idx][nibble]);
        outbuf[sample_count] = hist;

        step_idx += index_table[nibble];
        if (step_idx < 0) { /* clip step */
            step_idx = 0;
        } else if (step_idx > 31) {
            step_idx = 31;
        }
    }

    /* update state */
    stream->adpcm_step_index = step_idx;
    stream->adpcm_history1_16 = hist;
}
