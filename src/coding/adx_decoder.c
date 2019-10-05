#include "coding.h"
#include "../util.h"

void decode_adx(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_size, coding_t coding_type) {
    uint8_t frame[0x12] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int scale, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (fixed size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 32 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    scale = get_16bitBE(frame+0x00);
    switch(coding_type) {
        case coding_CRI_ADX:
            scale = scale + 1;
            coef1 = stream->adpcm_coef[0];
            coef2 = stream->adpcm_coef[1];
            break;
        case coding_CRI_ADX_exp:
            scale = 1 << (12 - scale);
            coef1 = stream->adpcm_coef[0];
            coef2 = stream->adpcm_coef[1];
            break;
        case coding_CRI_ADX_fixed:
            scale = (scale & 0x1fff) + 1;
            coef1 = stream->adpcm_coef[(frame[0] >> 5)*2 + 0];
            coef2 = stream->adpcm_coef[(frame[0] >> 5)*2 + 1];
            break;
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            scale = ((scale ^ stream->adx_xor) & 0x1fff) + 1;
            coef1 = stream->adpcm_coef[0];
            coef2 = stream->adpcm_coef[1];
            break;
        default:
            scale = scale + 1;
            coef1 = stream->adpcm_coef[0];
            coef2 = stream->adpcm_coef[1];
            break;
    }

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample = 0;
        uint8_t nibbles = frame[0x02 + i/2];

        sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles):
                get_high_nibble_signed(nibbles);
        sample = sample * scale + (coef1 * hist1 >> 12) + (coef2 * hist2 >> 12);
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;

    if ((coding_type == coding_CRI_ADX_enc_8 || coding_type == coding_CRI_ADX_enc_9) && !(i % 32)) {
        for (i =0; i < stream->adx_channels; i++) {
            adx_next_key(stream);
        }
    }
}

void adx_next_key(VGMSTREAMCHANNEL * stream) {
    stream->adx_xor = (stream->adx_xor * stream->adx_mult + stream->adx_add) & 0x7fff;
}
