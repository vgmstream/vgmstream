#include "coding.h"
#include "../util.h"


/* Nintendo GC Disc TracK streaming ADPCM (similar to CD-XA) */
void decode_ngc_dtk(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t coef_index, shift_factor;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (fixed size), stereo */
    bytes_per_frame = 0x20;
    samples_per_frame = 28;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame L/R header (repeated at 0x03/04) */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    coef_index   = ((uint8_t)read_8bit(frame_offset+channel,stream->streamfile) >> 4) & 0xf;
    shift_factor = ((uint8_t)read_8bit(frame_offset+channel,stream->streamfile) >> 0) & 0xf;
    /* rare but happens, also repeated headers don't match (ex. Ikaruga (GC) SONG02.adp) */
    VGM_ASSERT_ONCE(coef_index > 4 || shift_factor > 12, "DTK: incorrect coefs/shift at %x\n", (uint32_t)frame_offset);

    /* decode nibbles */
    for (i = first_sample; i < first_sample+samples_to_do; i++) {
        int32_t hist = 0, new_sample;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x04+i,stream->streamfile);

        /* apply XA filters << 6 */
        switch(coef_index) {
            case 0:
                hist = 0; // (hist1 * 0) - (hist2 * 0);
                break;
            case 1:
                hist = (hist1 * 60); // - (hist2 * 0);
                break;
            case 2:
                hist = (hist1 * 115) - (hist2 * 52);
                break;
            case 3:
                hist = (hist1 * 98) - (hist2 * 55);
                break;
        }
        hist = (hist + 32) >> 6;
        if (hist >  0x1fffff) hist =  0x1fffff;
        if (hist < -0x200000) hist = -0x200000;

        new_sample = (channel==0) ? /* L=low nibble first */
                get_low_nibble_signed(nibbles) :
                get_high_nibble_signed(nibbles);
        new_sample = (new_sample << 12) >> shift_factor;
        new_sample = (new_sample << 6) + hist;

        hist2 = hist1;
        hist1 = new_sample;

        outbuf[sample_count] = clamp16(new_sample >> 6);
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
