#include "coding.h"
#include "../util.h"

static const int32_t l5_scales[32] = {
    0x00001000, 0x0000144E, 0x000019C5, 0x000020B4, 0x00002981, 0x000034AC, 0x000042D9, 0x000054D6,
    0x00006BAB, 0x000088A4, 0x0000AD69, 0x0000DC13, 0x0001174C, 0x00016275, 0x0001C1D8, 0x00023AE5,
    0x0002D486, 0x0003977E, 0x00048EEE, 0x0005C8F3, 0x00075779, 0x0009513E, 0x000BD31C, 0x000F01B5,
    0x00130B82, 0x00182B83, 0x001EAC92, 0x0026EDB2, 0x00316777, 0x003EB2E6, 0x004F9232, 0x0064FBD1
};

void decode_l5_555(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x12] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint16_t header;
    uint8_t coef_index;

    int16_t hist1 = stream->adpcm_history1_16;
    int16_t hist2 = stream->adpcm_history2_16;
    int16_t hist3 = stream->adpcm_history3_16;
    int32_t coef1, coef2, coef3;
    int32_t pos_scale, neg_scale;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x12;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 32 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    header = get_u32le(frame);
    coef_index = (header >> 10) & 0x1f;
    pos_scale = l5_scales[(header >> 5) & 0x1f];
    neg_scale = l5_scales[(header >> 0) & 0x1f];

    coef1 = stream->adpcm_coef_3by32[coef_index * 3 + 0];
    coef2 = stream->adpcm_coef_3by32[coef_index * 3 + 1];
    coef3 = stream->adpcm_coef_3by32[coef_index * 3 + 2];

    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t prediction, sample = 0;
        uint8_t nibbles = frame[0x02 + i/2];

        sample = (i&1) ?
                get_low_nibble_signed(nibbles):
                get_high_nibble_signed(nibbles);
        prediction = -(hist1 * coef1 + hist2 * coef2 + hist3 * coef3);

        if (sample >= 0)
            sample = (prediction + sample * pos_scale) >> 12;
        else
            sample = (prediction + sample * neg_scale) >> 12;
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist3 = hist2;
        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
    stream->adpcm_history3_16 = hist3;
}
