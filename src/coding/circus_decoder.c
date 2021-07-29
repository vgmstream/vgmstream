#include "coding.h"
#include "circus_decoder_lib.h"



struct circus_codec_data {
    STREAMFILE* sf;
    int16_t* buf;
    int buf_samples_all;
    circus_handle_t* handle;
};


circus_codec_data* init_circus_vq(STREAMFILE* sf, off_t start, uint8_t codec, uint8_t flags) {
    circus_codec_data* data = NULL;

    data = calloc(1, sizeof(circus_codec_data));
    if (!data) goto fail;

    data->sf = reopen_streamfile(sf, 0);
    data->handle = circus_init(start, codec, flags);
    if (!data->handle) goto fail;

    return data;
fail:
    free_circus_vq(data);
    return NULL;
}

void decode_circus_vq(circus_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channels) {
    int ok, i, samples_to_get;

    while (samples_to_do > 0) {
        if (data->buf_samples_all == 0) {
            ok = circus_decode_frame(data->handle, data->sf, &data->buf, &data->buf_samples_all);
            if (!ok) goto decode_fail;
        }

        samples_to_get = data->buf_samples_all / channels;
        if (samples_to_get > samples_to_do)
            samples_to_get = samples_to_do;

        for (i = 0; i < samples_to_get * channels; i++) {
            outbuf[i] = data->buf[i];
        }

        data->buf += samples_to_get * channels;
        data->buf_samples_all -= samples_to_get * channels;
        outbuf += samples_to_get * channels;
        samples_to_do -= samples_to_get;
    }

    return;
    
decode_fail:
    VGM_LOG("CIRCUS: decode error\n");
    memset(outbuf, 0, samples_to_do * channels * sizeof(sample_t));
}

void reset_circus_vq(circus_codec_data* data) {
    if (!data) return;

    circus_reset(data->handle);
    data->buf_samples_all = 0;
}

void seek_circus_vq(circus_codec_data* data, int32_t num_sample) {
    if (!data) return;

    reset_circus_vq(data);
    //data->samples_discard = num_sample; //todo (xpcm don't have loop points tho)
}

void free_circus_vq(circus_codec_data* data) {
    if (!data) return;

    close_streamfile(data->sf);
    circus_free(data->handle);
    free(data);
}

/* ************************************************************************* */

/* Circus XPCM mode 2 decoding, verified vs EF.exe (info from foo_adpcm/libpcm and https://github.com/lioncash/ExtractData) */
void decode_circus_adpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_pos = 0;
    int32_t hist = stream->adpcm_history1_32;
    int scale = stream->adpcm_scale;
    off_t frame_offset = stream->offset; /* frame size is 1 */


    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int8_t code = read_u8(frame_offset+i,stream->streamfile);

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
        sample_pos += channelspacing;
    }

    stream->adpcm_history1_32 = hist;
    stream->adpcm_scale = scale;
}
