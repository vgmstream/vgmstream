#include "coding.h"
#include "../util.h"

/* lsf ADPCM, as seen in Fastlane Street Racing */

static const short lsf_coefs[5][2] = {
    {0x73, -0x34},
    {0, 0},
    {0x62, -0x37},
    {0x3C, 0},
    {0x7A, -0x3c}
};

void decode_lsf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i=first_sample;
    int32_t sample_count;
    const int bytes_per_frame = 0x1c;
    const int samples_per_frame = (bytes_per_frame-1)*2;

    int framesin = first_sample/samples_per_frame;

    uint8_t q = 0xFF - read_8bit(framesin*bytes_per_frame + stream->offset,stream->streamfile);
    int scale = (q&0xF0)>>4;
    int coef_idx = q&0x0F;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;

    first_sample = first_sample%samples_per_frame;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(framesin*bytes_per_frame+stream->offset+1+i/2,stream->streamfile);

        long prediction =
            (hist1 * lsf_coefs[coef_idx][0] +
             hist2 * lsf_coefs[coef_idx][1]) / 0x40;

        prediction += (i&1?
            get_high_nibble_signed(sample_byte):
            get_low_nibble_signed(sample_byte)
        ) * (1 << (12-scale));

        prediction = clamp16(prediction);

        hist2 = hist1;
        hist1 = prediction;

        outbuf[sample_count] = prediction;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}
