#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/relic_lib.h"


typedef struct {
    relic_handle_t* handle;
    int channels;
    int frame_size;

    float fbuf[RELIC_SAMPLES_PER_FRAME * RELIC_MAX_CHANNELS];

    int32_t discard;
} relic_codec_data;


static void free_relic(void* priv_data) {
    relic_codec_data* data = priv_data;
    if (!data) return;

    relic_free(data->handle);
    free(data);
}

void* init_relic(int channels, int bitrate, int codec_rate) {
    relic_codec_data* data = NULL;

    if (channels > RELIC_MAX_CHANNELS)
        goto fail;

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

static int decode_frame_channels(VGMSTREAMCHANNEL* stream, relic_codec_data* data) {
    uint8_t buf[RELIC_BUFFER_SIZE];

    for (int ch = 0; ch < data->channels; ch++) {
        int bytes = read_streamfile(buf, stream->offset, data->frame_size, stream->streamfile);

        stream->offset += data->frame_size;

        if (bytes != data->frame_size) return -1;

        int ok = relic_decode_frame(data->handle, buf, ch);
        if (!ok) return -1;
    }

    return RELIC_SAMPLES_PER_FRAME;
}

static bool decode_frame_relic(VGMSTREAM* v) {
    decode_state_t* ds = v->decode_state;
    relic_codec_data* data = v->codec_data;

    int samples = decode_frame_channels(&v->ch[0], data);
    if (samples <= 0)
        return false;

    relic_get_float(data->handle, data->fbuf);

    sbuf_init_f16(&ds->sbuf, data->fbuf, samples, data->channels);
    ds->sbuf.filled = samples;

    if (data->discard) {
        ds->discard = data->discard;
        data->discard = 0;
    }

    return true;
}

static void reset_relic(void* priv_data) {
    relic_codec_data* data = priv_data;
    if (!data) return;

    relic_reset(data->handle);
    data->discard = 0;
}

static void seek_relic(VGMSTREAM* v, int32_t num_sample) {
    relic_codec_data* data = v->codec_data;
    if (!data) return;

    reset_relic(data);
    data->discard = num_sample;
}

int32_t relic_bytes_to_samples(size_t bytes, int channels, int bitrate) {
    return bytes / channels / (bitrate / 8) * RELIC_SAMPLES_PER_FRAME;
}

const codec_info_t relic_decoder = {
    .sample_type = SFMT_F16,
    .decode_frame = decode_frame_relic,
    .free = free_relic,
    .reset = reset_relic,
    .seek = seek_relic,
};
