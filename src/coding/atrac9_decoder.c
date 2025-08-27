#ifdef VGM_USE_ATRAC9
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libatrac9.h"


/* opaque struct */
typedef struct {
    uint8_t* buf;
    int buf_size;

    int16_t* sbuf;
    int discard;

    atrac9_config config;
    Atrac9CodecInfo info;
    void* handle;
} atrac9_codec_data;


static void free_atrac9(void* priv_data) {
    atrac9_codec_data* data = priv_data;
    if (!data) return;

    if (data->handle) Atrac9ReleaseHandle(data->handle);
    free(data->buf);
    free(data->sbuf);
    free(data);
}

void* init_atrac9(atrac9_config* cfg) {
    int status;
    uint8_t config_data[4];
    atrac9_codec_data* data = NULL;

    data = calloc(1, sizeof(atrac9_codec_data));
    if (!data) goto fail;

    data->handle = Atrac9GetHandle();
    if (!data->handle) goto fail;

    put_u32be(config_data, cfg->config_data);
    status = Atrac9InitDecoder(data->handle, config_data);
    if (status < 0) goto fail;

    status = Atrac9GetCodecInfo(data->handle, &data->info);
    if (status < 0) goto fail;
    //;VGM_LOG("ATRAC9: config=%x, sf-size=%x, sub-frames=%i x %i samples\n", cfg->config_data, data->info.superframeSize, data->info.framesInSuperframe, data->info.frameSamples);

    if (cfg->channels && cfg->channels != data->info.channels) {
        VGM_LOG("ATRAC9: channels in header %i vs config %i don't match\n", cfg->channels, data->info.channels);
        goto fail; /* unknown multichannel layout */
    }


    // must hold at least one superframe and its samples
    data->buf_size = data->info.superframeSize;

    // extra leeway as Atrac9Decode seems to overread ~2 bytes (doesn't affect decoding though)
    data->buf = calloc(data->buf_size + 0x10, sizeof(uint8_t));
    if (!data->buf) goto fail;

    // while ATRAC9 uses float internally, Sony's API only returns PCM16
    data->sbuf = calloc(data->info.channels * data->info.frameSamples * data->info.framesInSuperframe, sizeof(int16_t));
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

    int to_read = data->info.superframeSize;
    int bytes = read_streamfile(data->buf, vs->offset, to_read, vs->streamfile);

    vs->offset += bytes;

    return (bytes == to_read);
}

static int decode(VGMSTREAM* v) {
    int channels = v->channels;
    atrac9_codec_data* data = v->codec_data;

    uint8_t* buf = data->buf;
    int16_t* sbuf = data->sbuf;

    // decode all frames in the superframe block
    int samples = 0;
    for (int iframe = 0; iframe < data->info.framesInSuperframe; iframe++) {
        int bytes_used = 0;

        int status = Atrac9Decode(data->handle, buf, sbuf, &bytes_used);
        if (status < 0)  {
            VGM_LOG("ATRAC): decode error %i\n", status);
            return false;
        }

        buf += bytes_used;
        sbuf += data->info.frameSamples * channels;
        samples += data->info.frameSamples;
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

    sbuf_init_s16(&ds->sbuf, data->sbuf, samples, v->channels);
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

#if 0
    /* reopen/flush, not needed as superframes decode separatedly and there is no carried state */
    {
        Atrac9ReleaseHandle(data->handle);
        data->handle = Atrac9GetHandle();
        if (!data->handle) return;

        uint8_t config_data[4];
        put_u32be(config_data, data->config.config_data);

        int status = Atrac9InitDecoder(data->handle, config_data);
        if (status < 0) goto fail;
    }
#endif
}

static void seek_atrac9(VGMSTREAM* v, int32_t num_sample) {
    atrac9_codec_data* data = v->codec_data;
    if (!data) return;

    reset_atrac9(data);

    // find closest offset to desired sample, and samples to discard after that offset to reach loop
    int32_t seek_sample = data->config.encoder_delay + num_sample;
    int32_t superframe_samples = data->info.frameSamples * data->info.framesInSuperframe;

    // decoded frames affect each other slightly, so move offset back to make PCM stable
    // and equivalent to a full discard loop
    int superframe_number = (seek_sample / superframe_samples); // closest
    int superframe_back = 1; // 1 seems enough (even when only 1 subframe in superframe)
    if (superframe_back > superframe_number)
        superframe_back = superframe_number;

    int32_t seek_discard = (seek_sample % superframe_samples) + (superframe_back * superframe_samples);
    off_t seek_offset  = (superframe_number - superframe_back) * data->info.superframeSize;

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

static int atrac9_parse_config(uint32_t config_data, int* p_sample_rate, int* p_channels, size_t* p_frame_size, size_t* p_samples_per_frame) {
    static const int sample_rate_table[16] = {
            11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
            44100, 48000, 64000, 88200, 96000,128000,176400,192000
    };
    static const int samples_power_table[16] = {
        6, 6, 7, 7, 7, 8, 8, 8,
        6, 6, 7, 7, 7, 8, 8, 8
    };
    static const int channel_table[8] = {
            1, 2, 2, 6, 8, 4, 0, 0
    };

    int superframe_size, frames_per_superframe, samples_per_frame, samples_per_superframe;
    uint32_t sync             = (config_data >> 24) & 0xff; /* 8b */
    uint8_t sample_rate_index = (config_data >> 20) & 0x0f; /* 4b */
    uint8_t channels_index    = (config_data >> 17) & 0x07; /* 3b */
    /* uint8_t validation bit = (config_data >> 16) & 0x01; */ /* 1b */
    size_t frame_size         = (config_data >>  5) & 0x7FF; /* 11b */
    size_t superframe_index   = (config_data >>  3) & 0x3; /* 2b */
    /* uint8_t unused         = (config_data >>  0) & 0x7);*/ /* 3b */

    superframe_size = ((frame_size+1) << superframe_index);
    frames_per_superframe = (1 << superframe_index);
    samples_per_frame = 1 << samples_power_table[sample_rate_index];
    samples_per_superframe = samples_per_frame * frames_per_superframe;

    if (sync != 0xFE)
        goto fail;
    if (p_sample_rate)
        *p_sample_rate = sample_rate_table[sample_rate_index];
    if (p_channels)
        *p_channels = channel_table[channels_index];
    if (p_frame_size)
        *p_frame_size = superframe_size;
    if (p_samples_per_frame)
        *p_samples_per_frame = samples_per_superframe;

    return 1;
fail:
    return 0;
}

size_t atrac9_bytes_to_samples(size_t bytes, void* priv_data) {
    atrac9_codec_data* data = priv_data;
    return bytes / data->info.superframeSize * (data->info.frameSamples * data->info.framesInSuperframe);
}

size_t atrac9_bytes_to_samples_cfg(size_t bytes, uint32_t config_data) {
    size_t frame_size, samples_per_frame;
    if (!atrac9_parse_config(config_data, NULL, NULL, &frame_size, &samples_per_frame))
        return 0;
    return bytes / frame_size * samples_per_frame;
}

const codec_info_t atrac9_decoder = {
    .sample_type = SFMT_S16, //TODO: decoder doesn't return float (to match Sony's lib apparently)
    .decode_frame = decode_frame_atrac9,
    .free = free_atrac9,
    .reset = reset_atrac9,
    .seek = seek_atrac9,
};
#endif
