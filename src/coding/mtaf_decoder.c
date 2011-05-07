//#include <stdlib.h>
//#include <stdio.h>
#include "coding.h"
#include "../util.h"

// A hybrid of IMA and Yamaha ADPCM found in Metal Gear Solid 3
// Currently this is all guesswork

int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
}; 

// This table is estimated by least-squares errors in the stored predictors
// .8 fixed point
int step_table[32] = {
    455,558,651,779,940,1131,1354,1613,1937,2300,2734,3277,3882,4652,5553,6607,7855,9357,11153,13312,15871,18862,22477,26796,31915,38030,45309,53931,64264,76554,91181,108629,
};

int delta_table[16] = {
    1,  3,  5,  7,  9, 11, 13, 15,
    -1, -3, -5, -7, -9,-11,-13,-15
};

// convert from .8 fixed to nearest int
static int16_t roundy(long v)
{
    if (v > 0)
    {
        return (v+0x80)/0x100;
    }
    else
    {
        return -(((-v)+0x80)/0x100);
    }
}

void decode_mtaf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int channels) {
    int32_t sample_count;
    int framesin = first_sample / 0x100;
    unsigned long cur_off = stream->offset + framesin*(0x10+0x80*2)*(channels/2);
    int i;
    int c = channel%2;
    int16_t init_idx = read_16bitLE(cur_off+4+c*2, stream->streamfile);
    int16_t init_hist = read_16bitLE(cur_off+8+c*4, stream->streamfile);
    int32_t hist = stream->adpcm_history1_32;
    int step_idx = stream->adpcm_step_index;

    //printf("channel %d: first_sample = %d, stream->offset = 0x%lx, cur_off = 0x%lx init_idx = %d\n", channel, first_sample, (unsigned long)stream->offset, cur_off, init_idx);

#if 0
    if (init_idx < 0 || init_idx > 31) {
        fprintf(stderr, "step idx out of range at 0x%lx ch %d\n", cur_off, c);
        exit(1);
    }
    if (0 != read_16bitLE(cur_off+10+c*4, stream->streamfile)) {
        fprintf(stderr, "exp. zero after hist at 0x%lx ch %d\n", cur_off, c);
        exit(1);
    }
#endif

    first_sample = first_sample%0x100;

    if (first_sample%0x100 == 0) {
        hist = init_hist*0x100;

#if 0
        if (step_idx != init_idx) {
            fprintf(stderr, "step_idx does not match at 0x%lx, %d!=%d\n",cur_off,step_idx, init_idx);
            exit(1);
        }
#endif
        step_idx = init_idx;
    }


    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t byte, nibble;
        byte = read_8bit(cur_off + 0x10 + 0x80*c + i/2, stream->streamfile);
        if (i%2!=1)
        {
            // low nibble first
            nibble = byte&0xf;
        }
        else
        {
            // high nibble last
            nibble = byte >> 4;
        }

        hist += delta_table[nibble] * step_table[step_idx];

        if (hist > INT16_MAX*0x100L)
        {
            hist = INT16_MAX*0x100L;
        }
        if (hist < INT16_MIN*0x100L)
        {
            hist = INT16_MIN*0x100L;
        }

        outbuf[sample_count] = roundy(hist);

        step_idx += index_table[nibble];
        if (step_idx < 0)
        {
            step_idx = 0;
        }
        if (step_idx > 31)
        {
            step_idx = 31;
        }
    } /* end sample loop */

    // update state
    stream->adpcm_step_index = step_idx;
    stream->adpcm_history1_32 = hist;

}
