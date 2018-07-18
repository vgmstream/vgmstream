#include "coding.h"


/* Decodec Argonaut's ASF ADPCM codec. Algorithm follows Croc2_asf2raw.exe, and the waveform
 * looks almost correct, but should reverse engineer asfcodec.adl (DLL) for accuracy. */
void decode_asf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint32_t shift, mode;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x11;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    shift = (read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    mode = (read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;

    /* decoder nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x01 + i/2,stream->streamfile);

        new_sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles):
                get_high_nibble_signed(nibbles);
        new_sample = new_sample << shift;

        switch(mode) {
            case 0x00:
                new_sample = new_sample + hist1;
                //new_sample = (new_sample + (hist1 << 6)) >> 6; /* maybe? */
                break;

            case 0x04:
                new_sample = new_sample + hist1*2 - hist2;
                //new_sample = (new_sample + (hist1 << 7) - (hist2 << 6)) >> 6; /* maybe? */
                break;

            default: /* other modes (ex 0x02/09) seem only at last frame as 0 */
                //VGM_LOG("ASF: unknown mode %x at %lx\n", mode,frame_offset);
                //new_sample = 0; /* maybe? */
                break;
        }

        //new_sample = clamp16(new_sample); /* must not */
        new_sample = new_sample & 0xFFFF; /* probably unnecessary */

        outbuf[sample_count] = new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
