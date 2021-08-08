#include "coding.h"
#include "relic_decoder_lib.h"

//TODO: fix looping

struct relic_codec_data {
    relic_handle_t* handle;
    int channels;
    int frame_size;

    int32_t samples_discard;
    int32_t samples_consumed;
    int32_t samples_filled;
};


relic_codec_data* init_relic(int channels, int bitrate, int codec_rate) {
    relic_codec_data* data = NULL;

    data = calloc(1, sizeof(relic_codec_data));
    if (!data) goto fail;

    data->handle = relic_init(channels, bitrate, codec_rate);
    if (!data->handle) goto fail;

    data->channels = channels;
    data->frame_size = relic_get_frame_size(data->handle);

    return data;
fail:
    free_relic(data);
    return NULL;
}

static int decode_frame_next(VGMSTREAMCHANNEL* stream, relic_codec_data* data) {
    int ch;
    int bytes;
    int ok;
    uint8_t buf[RELIC_BUFFER_SIZE];

    for (ch = 0; ch < data->channels; ch++) {
        bytes = read_streamfile(buf, stream->offset, data->frame_size, stream->streamfile);
        if (bytes != data->frame_size) goto fail;
        stream->offset += data->frame_size;

        ok = relic_decode_frame(data->handle, buf, ch);
        if (!ok) goto fail;
    }

    data->samples_consumed = 0;
    data->samples_filled = RELIC_SAMPLES_PER_FRAME;
    return 1;
fail:
    return 0;
}

void decode_relic(VGMSTREAMCHANNEL* stream, relic_codec_data* data, sample_t* outbuf, int32_t samples_to_do) {

    while (samples_to_do > 0) {

        if (data->samples_consumed < data->samples_filled) {
            /* consume samples */
            int samples_to_get = (data->samples_filled - data->samples_consumed);

            if (data->samples_discard) {
                /* discard samples for looping */
                if (samples_to_get > data->samples_discard)
                    samples_to_get = data->samples_discard;
                data->samples_discard -= samples_to_get;
            }
            else {
                /* get max samples and copy */
                if (samples_to_get > samples_to_do)
                    samples_to_get = samples_to_do;

                relic_get_pcm16(data->handle, outbuf, samples_to_get, data->samples_consumed);

                samples_to_do -= samples_to_get;
                outbuf += samples_to_get * data->channels;
            }

            /* mark consumed samples */
            data->samples_consumed += samples_to_get;
        }
        else {
            int ok = decode_frame_next(stream, data);
            if (!ok) goto decode_fail;
        }
    }
    return;

decode_fail:
    /* on error just put some 0 samples */
    VGM_LOG("RELIC: decode fail, missing %i samples\n", samples_to_do);
    memset(outbuf, 0, samples_to_do * data->channels * sizeof(sample));
}

void reset_relic(relic_codec_data* data) {
    if (!data) return;

    relic_reset(data->handle);
    data->samples_filled = 0;
    data->samples_consumed = 0;
    data->samples_discard = 0;
}

void seek_relic(relic_codec_data* data, int32_t num_sample) {
    if (!data) return;

    reset_relic(data);
    data->samples_discard = num_sample;
}

void free_relic(relic_codec_data* data) {
    if (!data) return;

    relic_free(data->handle);
    free(data);
}

int32_t relic_bytes_to_samples(size_t bytes, int channels, int bitrate) {
    return bytes / channels / (bitrate / 8) * RELIC_SAMPLES_PER_FRAME;
}
