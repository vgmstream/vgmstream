#ifdef VGM_USE_ATRAC9
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/atrac9_dec.h"


/* opaque struct */
typedef struct {
    uint8_t* buf;
    int buf_size;

    float* sbuf;
    int discard;

    atrac9_config config;
    atrac9_info_t info;
    void* handle;
} atrac9_codec_data;


static void free_atrac9(void* priv_data) {
    atrac9_codec_data* data = priv_data;
    if (!data) return;

    atrac9_free(data->handle);
    free(data->buf);
    free(data->sbuf);
    free(data);
}

void* init_atrac9(atrac9_config* cfg) {
    int res;
    uint8_t config_data[4];
    atrac9_codec_data* data = NULL;

    data = calloc(1, sizeof(atrac9_codec_data));
    if (!data) goto fail;

    put_u32be(config_data, cfg->config_data);
    data->handle = atrac9_init(config_data);
    if (!data->handle) goto fail;

    res = atrac9_get_info(data->handle, &data->info);
    if (res < 0) goto fail;


    if (cfg->channels && cfg->channels != data->info.channels) {
        VGM_LOG("ATRAC9: channels in header %i vs config %i don't match\n", cfg->channels, data->info.channels);
        goto fail; /* unknown multichannel layout */
    }

    if (data->info.dmc_mode) {
        // should work but not seen in the wild
        VGM_LOG("ATRAC9: DMC mode found\n");
        goto fail;
    }


    // must hold at least one superframe and its samples
    data->buf_size = data->info.superframe_size;
    data->buf = calloc(data->buf_size, sizeof(uint8_t));
    if (!data->buf) goto fail;

    data->sbuf = calloc(data->info.channels * data->info.frame_samples * data->info.frames_per_superframe, sizeof(float));
    if (!data->sbuf) goto fail;

    data->discard = cfg->encoder_delay;

    memcpy(&data->config, cfg, sizeof(atrac9_config));

    return data;

fail:
    free_atrac9(data);
    return NULL;
}



// read one raw block (superframe) and advance offsets; CBR and around 0x100-200 bytes
static bool read_frame(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* vs = &v->ch[0];
    atrac9_codec_data* data = v->codec_data;

    int bytes = read_streamfile(data->buf, vs->offset, data->buf_size, vs->streamfile);

    vs->offset += bytes;

    return (bytes == data->buf_size);
}

static int decode(VGMSTREAM* v) {
    atrac9_codec_data* data = v->codec_data;

    int samples = atrac9_decode_superframe(data->handle, data->buf, data->buf_size, data->sbuf);
    if (samples < 0)  {
        VGM_LOG("ATRAC9: decode error %i\n", samples);
        return false;
    }

    return samples;
}

// ATRAC9 is made of decodable superframes with several sub-frames. AT9 config data gives
// superframe size, number of frames and samples (~100-200 bytes and ~256/1024 samples).
static bool decode_frame_atrac9(VGMSTREAM* v) {
    bool ok = read_frame(v);
    if (!ok)
        return false;

    int samples = decode(v);
    if (samples <= 0)
        return false;

    decode_state_t* ds = v->decode_state;
    atrac9_codec_data* data = v->codec_data;

    sbuf_init_f16(&ds->sbuf, data->sbuf, samples, v->channels);
    ds->sbuf.filled = samples;

    if (data->discard) {
        ds->discard += data->discard;
        data->discard = 0;
    }

    return true;
}

static void reset_atrac9(void* priv_data) {
    atrac9_codec_data* data = priv_data;
    if (!data || !data->handle)
        return;

    data->discard = data->config.encoder_delay;

    // seemingly not done in OG lib, but only affects overlap
    //atrac9_reset(atrac9_handle_t* handle)
}

static void seek_atrac9(VGMSTREAM* v, int32_t num_sample) {
    atrac9_codec_data* data = v->codec_data;
    if (!data) return;

    reset_atrac9(data);

    // find closest offset to desired sample, and samples to discard after that offset to reach loop
    int32_t seek_sample = data->config.encoder_delay + num_sample;
    int32_t superframe_samples = data->info.frame_samples * data->info.frames_per_superframe;

    // decoded frames affect each other slightly, so move offset back to make PCM stable
    // and equivalent to a full discard loop
    int superframe_number = (seek_sample / superframe_samples); // closest
    int superframe_back = 1; // 1 seems enough (even when only 1 subframe in superframe)
    if (superframe_back > superframe_number)
        superframe_back = superframe_number;

    int32_t seek_discard = (seek_sample % superframe_samples) + (superframe_back * superframe_samples);
    off_t seek_offset  = (superframe_number - superframe_back) * data->info.superframe_size;

    data->discard = seek_discard; // already includes encoder delay

    if (v->loop_ch) {
        v->loop_ch[0].offset = v->loop_ch[0].channel_start_offset + seek_offset;
    }

#if 0
    //old full discard loop
    data->discard = num_sample;
    data->discard += data->config.encoder_delay;

    // loop offsets are set during decode; force them to stream start so discard works
    if (vgmstream->loop_ch)
        vgmstream->loop_ch[0].offset = vgmstream->loop_ch[0].channel_start_offset;
#endif
}

size_t atrac9_bytes_to_samples(size_t bytes, void* priv_data) {
    atrac9_codec_data* data = priv_data;
    return bytes / data->info.superframe_size * (data->info.frame_samples * data->info.frames_per_superframe);
}

size_t atrac9_bytes_to_samples_cfg(size_t bytes, uint32_t config_data) {
    atrac9_info_t info = {0};

    uint8_t buf_config_data[4];
    put_u32be(buf_config_data, config_data);

    int res = atrac9_get_config_info(buf_config_data, &info);
    if (res < 0)
        return 0;

    size_t frame_size = info.superframe_size;
    size_t samples_per_frame = info.frames_per_superframe * info.frame_samples;
    return bytes / frame_size * samples_per_frame;
}

const codec_info_t atrac9_decoder = {
    .sample_type = SFMT_F16,
    .decode_frame = decode_frame_atrac9,
    .free = free_atrac9,
    .reset = reset_atrac9,
    .seek = seek_atrac9,
};
#endif
