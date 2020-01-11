#include "coding.h"


/* tweaked XA/PSX coefs << 6 */
static const int8_t fadpcm_coefs[8][2] = {
        {   0,   0 },
        {  60,   0 },
        { 122,  60 },
        { 115,  52 },
        {  98,  55 },
        /* rest is 0s */
};

/* FMOD's FADPCM, basically XA/PSX ADPCM with a fancy header layout.
 * Code/layout could be simplified but tries to emulate FMOD's code.
 * Algorithm and tables debugged from their PC DLLs (byte-accurate). */
void decode_fadpcm(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x8c] = {0};
    off_t frame_offset;
    int i, j, k, frames_in, sample_count = 0, samples_done = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint32_t coefs, shifts;
    int32_t hist1; //= stream->adpcm_history1_32;
    int32_t hist2; //= stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x8c;
    samples_per_frame = (bytes_per_frame - 0xc) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse 0xc header (header samples are not written to outbuf) */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    coefs  = get_u32le(frame + 0x00);
    shifts = get_u32le(frame + 0x04);
    hist1  = get_s16le(frame + 0x08);
    hist2  = get_s16le(frame + 0x0a);


    /* decode nibbles, grouped in 8 sets of 0x10 * 0x04 * 2 */
    for (i = 0; i < 8; i++) {
        int index, shift, coef1, coef2;

        /* each set has its own coefs/shifts (indexes > 7 are repeat, ex. 0x9 is 0x2) */
        index = ((coefs >> i*4) & 0x0f) % 0x07;
        shift = (shifts >> i*4) & 0x0f;

        coef1 = fadpcm_coefs[index][0];
        coef2 = fadpcm_coefs[index][1];
        shift = 22 - shift; /* pre-adjust for 32b sign extend */

        for (j = 0; j < 4; j++) {
            uint32_t nibbles = get_u32le(frame + 0x0c + 0x10*i + 0x04*j);

            for (k = 0; k < 8; k++) {
                int32_t sample;

                sample = (nibbles >> k*4) & 0x0f;
                sample = (sample << 28) >> shift; /* 32b sign extend + scale */
                sample = (sample - hist2*coef2 + hist1*coef1) >> 6;
                sample = clamp16(sample);

                if (sample_count >= first_sample && samples_done < samples_to_do) {
                    outbuf[samples_done * channelspacing] = sample;
                    samples_done++;
                }
                sample_count++;

                hist2 = hist1;
                hist1 = sample;
            }
        }
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_history2_32 = hist2;
}
