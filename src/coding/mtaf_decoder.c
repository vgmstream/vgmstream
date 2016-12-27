//#include <stdlib.h>
//#include <stdio.h>
#include "coding.h"
#include "../util.h"

#define MTAF_BLOCK_SUPPORT 0

// A hybrid of IMA and Yamaha ADPCM found in Metal Gear Solid 3
// Thanks to X_Tra (http://metalgear.in/) for pointing me to the step size table.

int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
}; 

static int16_t step_size[32][16] = {
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


void decode_mtaf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int channels) {
    int32_t sample_count;
    off_t cur_off = stream->offset;
    int i;
    int c = channel%2; /* global channel to stream channel */
    int32_t hist = stream->adpcm_history1_16;
    int32_t step_idx = stream->adpcm_step_index;
	uint8_t byte = 0;


#if MTAF_BLOCK_SUPPORT
    {
        /* "macroblock" support (layout/mtaf_block.c) was removed since the extractor now produces clean files;
         *   this a hack to skip those blocks, left as a reminder (not well tested) */
        int unk, size, empty, frames, repeat = 1;
        do {
            unk    = read_32bitLE(cur_off+0x00, stream->streamfile); /* always BE 0x01001100? */
            size   = read_32bitLE(cur_off+0x04, stream->streamfile); /* block size */
            empty  = read_32bitLE(cur_off+0x08, stream->streamfile); /* always 0? */
            frames = read_32bitLE(cur_off+0x0c, stream->streamfile); /* total frames of 0x110 */
            if (unk == 0x00110001 && empty == 0 && size > 0) {
                if (frames == 0) {
                    stream->offset += size; /* full skip */
                } else if ((size-0x10) == frames*0x110) {
                    stream->offset += 0x10; /* header skip */
                    repeat = 0;
                }
                cur_off = stream->offset;
            }
            else {
                repeat = 0;
            }

        } while(repeat);
    }
#endif

    first_sample = first_sample % 0x100;

    /* read header when we hit a new frame every 0x100 samples */
    if (first_sample == 0) {
        int32_t init_idx, init_hist;

		/*  0x10 header: owner stream, frame count, step-L, step-R, hist-L, hist-R */
        /* uint32_t stream = read_8bit(cur_off+0+c*2, stream->streamfile); */ /* 0=first */
        /* uint24_t frames = (uint24_t)read_16bitLE(cur_off+1, stream->streamfile); */ /* 1=first */
        init_idx  = read_16bitLE(cur_off+4+c*2, stream->streamfile); /* step-L/R */
        init_hist = read_16bitLE(cur_off+4+4+c*4, stream->streamfile); /* hist-L/R: hist 16bit + empty 16bit */

        VGM_ASSERT( read_16bitLE(cur_off+4+4+2+c*4, stream->streamfile) != 0,
                "init_hist not 16bit at 0x%lx, ch=%d\n", cur_off, c);
        VGM_ASSERT( init_idx < 0 || init_idx > 31,
                "init_idx out of range at 0x%lx, ch=%d\n", cur_off, c);
        VGM_ASSERT( step_idx != init_idx,
                "step_idx does not match init_idx at 0x%lx, step=%d, init=%d\n",cur_off,step_idx, init_idx);

        /* avoid index out of range in corrupt files */
        if (init_idx < 0) {
            init_idx = 0;
        } else if (init_idx > 31) {
            init_idx = 31;
        }

        step_idx = init_idx;
        hist = init_hist;
    }


    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t nibble;

        if (i%2 != 1) { /* low nibble first */
            byte = read_8bit(cur_off + 0x10 + 0x80*c + i/2, stream->streamfile);
            nibble = byte & 0x0f;
        } else { /* high nibble last */
            nibble = byte >> 4;
        }

        hist = clamp16(hist+step_size[step_idx][nibble]);

        outbuf[sample_count] = hist;

        step_idx += index_table[nibble];
        if (step_idx < 0) { /* clip step */
            step_idx = 0;
        } else if (step_idx > 31) {
            step_idx = 31;
        }
    } /* end sample loop */

    // update state
    stream->adpcm_step_index = step_idx;
    stream->adpcm_history1_16 = hist;

}
