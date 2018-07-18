#include "coding.h"


/* FADPCM table */
static const int8_t fadpcm_coefs[8][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 122 ,  60 },
        { 115 ,  52 },
        {  98 ,  55 },
        {   0 ,   0 },
        {   0 ,   0 },
        {   0 ,   0 },
};

/* FMOD's FADPCM, basically XA/PSX ADPCM with a fancy header layout.
 * Code/layout could be simplified but tries to emulate FMOD's code.
 * Algorithm and tables debugged from their PC DLLs (byte-accurate). */
void decode_fadpcm(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    off_t frame_offset;
    int i, j, k;
    int block_samples, frames_in, samples_done = 0, sample_count = 0;
    uint32_t coefs, shifts;
    int32_t hist1; //= stream->adpcm_history1_32;
    int32_t hist2; //= stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    block_samples = (0x8c - 0xc) * 2;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    frame_offset = stream->offset + 0x8c*frames_in;


    /* parse 0xc header (header samples are not written to outbuf) */
    coefs  = read_32bitLE(frame_offset + 0x00, stream->streamfile);
    shifts = read_32bitLE(frame_offset + 0x04, stream->streamfile);
    hist1  = read_16bitLE(frame_offset + 0x08, stream->streamfile);
    hist2  = read_16bitLE(frame_offset + 0x0a, stream->streamfile);


    /* decode nibbles, grouped in 8 sets of 0x10 * 0x04 * 2 */
    for (i = 0; i < 8; i++) {
        int32_t coef1, coef2, shift, coef_index, shift_factor;
        off_t group_offset = frame_offset + 0x0c + 0x10*i;

        /* each set has its own coefs/shifts (indexes > 7 are repeat, ex. 0x9 is 0x2) */
        coef_index   = ((coefs >> i*4) & 0x0f) % 0x07;
        shift_factor = (shifts >> i*4) & 0x0f;

        coef1 = fadpcm_coefs[coef_index][0];
        coef2 = fadpcm_coefs[coef_index][1];
        shift = 0x16 - shift_factor; /* pre-adjust for 32b sign extend */

        for (j = 0; j < 4; j++) {
            uint32_t nibbles = read_32bitLE(group_offset + 0x04*j, stream->streamfile);

            for (k = 0; k < 8; k++) {
                int32_t new_sample;

                new_sample = (nibbles >> k*4) & 0x0f;
                new_sample = (new_sample << 28) >> shift; /* 32b sign extend + scale */
                new_sample = (new_sample - hist2*coef2 + hist1*coef1);
                new_sample = new_sample >> 6;
                new_sample = clamp16(new_sample);

                if (sample_count >= first_sample && samples_done < samples_to_do) {
                    outbuf[samples_done * channelspacing] = new_sample;
                    samples_done++;
                }
                sample_count++;

                hist2 = hist1;
                hist1 = new_sample;
            }
        }
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_history2_32 = hist2;
}
