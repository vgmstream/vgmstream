#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/ka1a_dec.h"


/* opaque struct */
typedef struct {
    uint8_t* buf;
    float* fbuf;

    int frame_size;
    void* handle;
} ka1a_codec_data;


static void free_ka1a(void* priv_data) {
    ka1a_codec_data* data = priv_data;
    if (!data) return;

    if (data->handle)
        ka1a_free(data->handle);
    free(data->buf);
    free(data->fbuf);
    free(data);
}

void* init_ka1a(int bitrate_mode, int channels_tracks) {
    ka1a_codec_data* data = NULL;
    int buf_size;

    data = calloc(1, sizeof(ka1a_codec_data));
    if (!data) goto fail;

    data->handle = ka1a_init(bitrate_mode, channels_tracks, 1);
    if (!data->handle) goto fail;

    data->frame_size = ka1a_get_frame_size(data->handle);
    if (data->frame_size <= 0) goto fail;

    buf_size = data->frame_size * channels_tracks;
    data->buf = calloc(buf_size, sizeof(uint8_t));
    if (!data->buf) goto fail;

    data->fbuf = calloc(KA1A_FRAME_SAMPLES * channels_tracks, sizeof(float));
    if (!data->fbuf) goto fail;

    return data;
fail:
    free_ka1a(data);
    return NULL;
}

static bool read_frame(VGMSTREAM* v) {
    ka1a_codec_data* data = v->codec_data;
    int bytes;

    if (v->codec_config) {
        int block = data->frame_size;

        // interleaved mode: read from each channel separately and mix in buf
        for (int ch = 0; ch < v->channels; ch++) {
            VGMSTREAMCHANNEL* vs = &v->ch[ch];

            bytes = read_streamfile(data->buf + block * ch, vs->offset, block, vs->streamfile);
            if (bytes != block)
                return false;

            vs->offset += bytes;
        }
    }
    else {
        // single block of frames
        int block = data->frame_size * v->channels;
        VGMSTREAMCHANNEL* vs = &v->ch[0];

        bytes = read_streamfile(data->buf, vs->offset, block, vs->streamfile);
        if (bytes != block)
            return false;

        vs->offset += bytes;
    }

    return true;
}

static bool decode_frame_ka1a(VGMSTREAM* v) {
    bool ok = read_frame(v);
    if (!ok)
        return false;

    decode_state_t* ds = v->decode_state;
    ka1a_codec_data* data = v->codec_data;

    int samples = ka1a_decode(data->handle, data->buf, data->fbuf);
    if (samples < 0)
        return false;

    sbuf_init_flt(&ds->sbuf, data->fbuf, KA1A_FRAME_SAMPLES, v->channels);
    ds->sbuf.filled = samples;

    return true;
}

static void reset_ka1a(void* priv_data) {
    ka1a_codec_data* data = priv_data;
    if (!data || !data->handle) return;
    
    ka1a_reset(data->handle);
}

static void seek_ka1a(VGMSTREAM* v, int32_t num_sample) {
    ka1a_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    if (!data) return;

    reset_ka1a(data);

    // find closest offset to desired sample
    int32_t seek_frame = num_sample / KA1A_FRAME_SAMPLES;
    int32_t seek_sample = num_sample % KA1A_FRAME_SAMPLES;

    ds->discard = seek_sample;

    if (v->codec_config) {
        uint32_t seek_offset = seek_frame * data->frame_size;

        if (v->loop_ch) {
            for (int ch = 0; ch < v->channels; ch++) {
                v->loop_ch[ch].offset = v->loop_ch[ch].channel_start_offset + seek_offset;
            }
        }
    }
    else {
        uint32_t seek_offset = seek_frame * data->frame_size * v->channels;

        if (v->loop_ch) {
            v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset + seek_offset;
        }
    }

    // (due to implicit encode delay the above is byte-exact equivalent vs a discard loop)
    #if 0
    ds->discard = num_sample;
    if (v->loop_ch) {
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset;
    }
    #endif
}

const codec_info_t ka1a_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_ka1a,
    .free = free_ka1a,
    .reset = reset_ka1a,
    .seek = seek_ka1a,
};
