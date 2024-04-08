#include "coding.h"


static int expand_code(uint8_t code) {
    int neg = code & 0x80;
    int cmd = code & 0x07;

    int v;
    if (cmd == 7)
        v = (code & 0x78) << 8;
    else
        v = ((code & 0x78) | 0x80) << 7;
    v = (v >> cmd);
    if (neg)
        v = -v;
    return v;
}

/* from the decompilation, mono mode seems to do this:
 *   hist_a += expand_code(codes[i++])
 *   hist_b += decode_byte(codes[i++])
 *   sample = (hist_a + hist_b) / 2;
 *   out[s++] = sample; //L
 *   out[s++] = sample; //R, repeated for to make fake stereo)
 * Existing files seem to be all stereo though
*/

/* decompiled from the exe */
void decode_dpcm_kcej(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    off_t frame_offset = stream->offset; /* frame size is 1 */
    int32_t hist = stream->adpcm_history1_32;

    int sample_pos = 0;
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = read_u8(frame_offset + i, stream->streamfile);
        
        hist += expand_code(code);

        outbuf[sample_pos] = hist; /* no clamp */
        sample_pos += channelspacing;
    }

    stream->adpcm_history1_32 = hist;
}
