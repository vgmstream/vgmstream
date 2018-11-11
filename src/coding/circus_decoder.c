#include "coding.h"


/* Circus XPCM mode 2 decoding, verified vs EF.exe (info from foo_adpcm/libpcm and https://github.com/lioncash/ExtractData) */
void decode_circus_adpcm(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_pos = 0;
    int32_t hist = stream->adpcm_history1_32;
    int scale = stream->adpcm_scale;
    off_t frame_offset = stream->offset; /* frame size is 1 */


    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int8_t code = read_8bit(frame_offset+i,stream->streamfile);

        hist += code << scale;
        if (code == 0) {
            if (scale > 0)
                scale--;
        }
        else if (code == 127 || code == -128) {
            if (scale < 8)
                scale++;
        }
        outbuf[sample_pos] = hist;
    }

    stream->adpcm_history1_32 = hist;
    stream->adpcm_scale = scale;
}
