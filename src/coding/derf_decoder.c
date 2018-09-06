#include "coding.h"


/* IMA table plus seven extra steps at the beginning */
static const int derf_steps[96] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 16,
        17, 19, 21, 23, 25, 28, 31, 34,
        37, 41, 45, 50, 55, 60, 66, 73,
        80, 88, 97, 107, 118, 130, 143, 157,
        173, 190, 209, 230, 253, 279, 307, 337,
        371, 408, 449, 494, 544, 598, 658, 724,
        796, 876, 963, 1060, 1166, 1282, 1411, 1552,
        1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
        3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
        7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

/* Xilam DERF DPCM for Stupid Invaders (PC), decompiled from the exe */
void decode_derf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_pos = 0, index;
    int32_t hist = stream->adpcm_history1_32;
    off_t frame_offset = stream->offset; /* frame size is 1 */

    for(i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = (uint8_t)read_8bit(frame_offset+i,stream->streamfile);

        /* original exe doesn't clamp the index, so presumably codes can't over it */
        index = code & 0x7f;
        if (index > 95) index = 95;

        if (code & 0x80)
            hist -= derf_steps[index];
        else
            hist += derf_steps[index];

        outbuf[sample_pos] = clamp16(hist);
        sample_pos += channelspacing;
    }

    stream->adpcm_history1_32 = hist;
}
