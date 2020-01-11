#include "coding.h"
#include "../util.h"


/* standard XA coefs << 6 */
static const int8_t dtk_coefs[16][2] = {
        {   0,  0 },
        {  60,  0 },
        { 115, 52 },
        {  98, 55 },
        /* rest assumed to be 0s */
};

/* Nintendo GC Disc TracK streaming ADPCM (similar to XA) */
void decode_ngc_dtk(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    uint8_t frame[0x20] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int index, shift, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (fixed size), stereo */
    bytes_per_frame = 0x20;
    samples_per_frame = (0x20 - 0x04); /* 28 for each channel */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame L/R header (repeated at 0x03/04) */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    index = (frame[channel] >> 4) & 0xf;
    shift = (frame[channel] >> 0) & 0xf;
    coef1 = dtk_coefs[index][0];
    coef2 = dtk_coefs[index][1];
    /* rare but happens, also repeated headers don't match (ex. Ikaruga (GC) SONG02.adp) */
    VGM_ASSERT_ONCE(index > 4 || shift > 12, "DTK: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int sample, hist;
        uint8_t nibbles = frame[0x04 + i];

        hist = (hist1*coef1 - hist2*coef2 + 32) >> 6;
        if (hist > 2097151) hist = 2097151;
        else if (hist < -2097152) hist = -2097152;

        sample = (channel==0) ? /* L=low nibble first */
                get_low_nibble_signed(nibbles) :
                get_high_nibble_signed(nibbles);
        sample = (sample << 12) >> shift;
        sample = (sample << 6) + hist;

        hist2 = hist1;
        hist1 = sample; /* clamp *after* this so hist goes pretty high */

        outbuf[sample_count] = clamp16(sample >> 6);
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
