#include "coding.h"
#include "../util.h"

void decode_adx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes) {
    int i;
    int32_t sample_count;
    int32_t frame_samples = (frame_bytes - 2) * 2;

    int framesin = first_sample/frame_samples;

    int32_t scale = read_16bitBE(stream->offset+framesin*frame_bytes,stream->streamfile) + 1;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int coef1 = stream->adpcm_coef[0];
    int coef2 = stream->adpcm_coef[1];

    first_sample = first_sample%frame_samples;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(stream->offset+framesin*frame_bytes +2+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16(
                (i&1?
                 get_low_nibble_signed(sample_byte):
                 get_high_nibble_signed(sample_byte)
                ) * scale +
                (coef1 * hist1 >> 12) + (coef2 * hist2 >> 12)
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

void decode_adx_exp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes) {
    int i;
    int32_t sample_count;
    int32_t frame_samples = (frame_bytes - 2) * 2;

    int framesin = first_sample/frame_samples;

    int32_t scale = read_16bitBE(stream->offset+framesin*frame_bytes,stream->streamfile);
    int32_t hist1, hist2;
    int coef1, coef2;
    scale = 1 << (12 - scale);
    hist1 = stream->adpcm_history1_32;
    hist2 = stream->adpcm_history2_32;
    coef1 = stream->adpcm_coef[0];
    coef2 = stream->adpcm_coef[1];

    first_sample = first_sample%frame_samples;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(stream->offset+framesin*frame_bytes +2+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16(
                (i&1?
                 get_low_nibble_signed(sample_byte):
                 get_high_nibble_signed(sample_byte)
                ) * scale +
                (coef1 * hist1 >> 12) + (coef2 * hist2 >> 12)
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

void decode_adx_fixed(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes) {
    int i;
    int32_t sample_count;
    int32_t frame_samples = (frame_bytes - 2) * 2;

    int framesin = first_sample/frame_samples;

    int32_t scale = (read_16bitBE(stream->offset + framesin*frame_bytes, stream->streamfile) & 0x1FFF) + 1;
    int32_t predictor = read_8bit(stream->offset + framesin*frame_bytes, stream->streamfile) >> 5;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int coef1 = stream->adpcm_coef[predictor * 2];
    int coef2 = stream->adpcm_coef[predictor * 2 + 1];

    first_sample = first_sample%frame_samples;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(stream->offset+framesin*frame_bytes +2+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16(
                (i&1?
                 get_low_nibble_signed(sample_byte):
                 get_high_nibble_signed(sample_byte)
                ) * scale +
                (coef1 * hist1 >> 12) + (coef2 * hist2 >> 12)
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

void adx_next_key(VGMSTREAMCHANNEL * stream)
{
    stream->adx_xor = ( stream->adx_xor * stream->adx_mult + stream->adx_add ) & 0x7fff;
}

void decode_adx_enc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes) {
    int i;
    int32_t sample_count;
    int32_t frame_samples = (frame_bytes - 2) * 2;

    int framesin = first_sample/frame_samples;

    int32_t scale = ((read_16bitBE(stream->offset+framesin*frame_bytes,stream->streamfile) ^ stream->adx_xor)&0x1fff) + 1;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int coef1 = stream->adpcm_coef[0];
    int coef2 = stream->adpcm_coef[1];

    first_sample = first_sample%frame_samples;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(stream->offset+framesin*frame_bytes +2+i/2,stream->streamfile);

        outbuf[sample_count] = clamp16(
                (i&1?
                 get_low_nibble_signed(sample_byte):
                 get_high_nibble_signed(sample_byte)
                ) * scale +
                (coef1 * hist1 >> 12) + (coef2 * hist2 >> 12)
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;

    if (!(i % 32)) {
        for (i=0;i<stream->adx_channels;i++)
        {
            adx_next_key(stream);
        }
    }

}
